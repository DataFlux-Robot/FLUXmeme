/* conv — .fluxa (text canonical source) <-> .flux (binary store).
 *
 * .fluxa format (line-oriented, diff-friendly):
 *   #FLUXMEME 1.0
 *   R <32-hex id>
 *   L BODY|MIND|JOURNAL
 *   K <kind>
 *   P <ptype>
 *   G <path>
 *   T <ts>
 *   C sim_time|wall_time|device_monotonic
 *   B TEXT|BIN
 *   M <key>=<val>            (repeatable; val escapes \ \n \r)
 *   N <32-hex target>=<rel>  (repeatable)
 *   D <len>                  (TEXT payload: next `len` bytes verbatim, then \n)
 *   X <hexlen>               (BIN payload: next `hexlen` hex chars, then \n)
 * See SPEC §1.5. */
#include "fluxmeme/fluxmeme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ---------- layer / clock names ---------- */
static const char* layer_str(int l) {
    if (l == FLUX_LAYER_BODY) return "BODY";
    if (l == FLUX_LAYER_MIND) return "MIND";
    if (l == FLUX_LAYER_JOURNAL) return "JOURNAL";
    return "NONE";
}
static int layer_from_str(const char* s) {
    if (strcmp(s, "BODY") == 0) return FLUX_LAYER_BODY;
    if (strcmp(s, "MIND") == 0) return FLUX_LAYER_MIND;
    if (strcmp(s, "JOURNAL") == 0) return FLUX_LAYER_JOURNAL;
    return 0;
}
static const char* clock_str(int c) {
    switch (c) {
    case FLUX_CLOCK_SIM_TIME: return "sim_time";
    case FLUX_CLOCK_WALL_TIME: return "wall_time";
    case FLUX_CLOCK_DEVICE_MONOTONIC: return "device_monotonic";
    }
    return "sim_time";
}
static int clock_from_str(const char* s) {
    if (strcmp(s, "wall_time") == 0) return FLUX_CLOCK_WALL_TIME;
    if (strcmp(s, "device_monotonic") == 0) return FLUX_CLOCK_DEVICE_MONOTONIC;
    return FLUX_CLOCK_SIM_TIME;
}

/* escape/unescape \, \n, \r for single-line meta val */
static void emit_escape(FILE* f, const char* s) {
    for (const char* p = s; *p; ++p) {
        if (*p == '\\') fputs("\\\\", f);
        else if (*p == '\n') fputs("\\n", f);
        else if (*p == '\r') fputs("\\r", f);
        else fputc(*p, f);
    }
}
/* unescape in place; returns new length */
static size_t unescape_inplace(char* s, size_t n) {
    size_t w = 0;
    for (size_t r = 0; r < n; ++r) {
        if (s[r] == '\\' && r + 1 < n) {
            char c = s[++r];
            s[w++] = (c == 'n') ? '\n' : (c == 'r') ? '\r' : c;
        } else {
            s[w++] = s[r];
        }
    }
    return w;
}

/* ---------- store -> fluxa ---------- */
flux_status_t flux_conv_to_fluxa(const flux_txn_t* txn, const char* out_fluxa) {
    if (!txn || !out_fluxa) return FLUX_ERR_ARG;
    FILE* f = fopen(out_fluxa, "wb");
    if (!f) return FLUX_ERR_IO;
    fprintf(f, "#FLUXMEME 1.0\n");

    flux_iter_t* it = NULL;
    if (flux_scan(txn, NULL, &it) != FLUX_OK) { fclose(f); return FLUX_ERR_IO; }
    flux_record_t r;
    while (flux_iter_next(it, &r) == FLUX_OK) {
        char hex[33];
        flux_id_to_hex(&r.id, hex);
        fprintf(f, "R %s\n", hex);
        fprintf(f, "L %s\n", layer_str(r.layer));
        if (r.kind) fprintf(f, "K %s\n", r.kind);
        if (r.ptype) fprintf(f, "P %s\n", r.ptype);
        if (r.path) fprintf(f, "G %s\n", r.path);
        fprintf(f, "T %llu\n", (unsigned long long)r.ts);
        fprintf(f, "C %s\n", clock_str(r.clock));
        fprintf(f, "B %s\n", r.pclass == FLUX_PCLASS_BIN ? "BIN" : "TEXT");
        for (uint32_t i = 0; i < r.meta_count; ++i) {
            fprintf(f, "M ");
            emit_escape(f, r.meta[i].key ? r.meta[i].key : "");
            fputc('=', f);
            emit_escape(f, r.meta[i].val ? r.meta[i].val : "");
            fputc('\n', f);
        }
        for (uint32_t i = 0; i < r.link_count; ++i) {
            char th[33];
            flux_id_to_hex(&r.links[i].target, th);
            fprintf(f, "N %s=%s\n", th, r.links[i].rel ? r.links[i].rel : "");
        }
        if (r.pclass == FLUX_PCLASS_BIN) {
            fprintf(f, "X %zu\n", r.payload.len * 2);
            for (size_t i = 0; i < r.payload.len; ++i)
                fprintf(f, "%02x", r.payload.data[i]);
            fputc('\n', f);
        } else {
            fprintf(f, "D %zu\n", r.payload.len);
            if (r.payload.len) fwrite(r.payload.data, 1, r.payload.len, f);
            fputc('\n', f);
        }
        flux_record_free(&r);
    }
    flux_iter_free(it);
    fclose(f);
    return FLUX_OK;
}

/* ---------- fluxa -> store ---------- */

/* read one line (up to \n) into a malloc'd buffer; *next set to after \n.
   returns line without trailing \n (len in *out_len), or NULL at end. */
static char* read_line(const char* s, size_t total, size_t* off, size_t* out_len) {
    if (*off >= total) return NULL;
    size_t start = *off, i = *off;
    while (i < total && s[i] != '\n') ++i;
    size_t len = i - start;
    char* line = (char*)malloc(len + 1);
    if (!line) return NULL;
    memcpy(line, s + start, len);
    line[len] = '\0';
    *out_len = len;
    *off = (i < total) ? i + 1 : total;
    return line;
}

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* parse hex bytes following an "X <hexlen>\n" line; reads hexlen hex chars. */
static void parse_hex_payload(const char* s, size_t total, size_t* off, size_t hexlen,
                              flux_record_t* rec) {
    uint8_t* buf = (uint8_t*)malloc(hexlen / 2 + 1);
    if (!buf) return;
    size_t n = 0, i = *off, k = 0;
    while (i < total && k + 1 < hexlen) {
        int hi = hexval((unsigned char)s[i]);
        int lo = hexval((unsigned char)s[i + 1]);
        if (hi < 0 || lo < 0) break;
        buf[n++] = (uint8_t)((hi << 4) | lo);
        k += 2;
        i += 2;
    }
    *off = i;
    /* skip to end of line */
    while (*off < total && s[*off] != '\n') (*off)++;
    if (*off < total) (*off)++;
    rec->payload.data = buf;
    rec->payload.len = n;
}

/* commit the in-progress record to the txn */
static void flush_rec(flux_record_t* rec, flux_meta_kv_t** meta, uint32_t* mc,
                      flux_link_t** links, uint32_t* lc, flux_txn_t* txn) {
    int id_nz = 0;
    for (int b = 0; b < 16; ++b)
        if (rec->id.bytes[b]) { id_nz = 1; break; }
    if (id_nz || *mc || *lc || rec->payload.len || rec->kind) {
        rec->meta = *meta;
        rec->meta_count = *mc;
        rec->links = *links;
        rec->link_count = *lc;
        flux_put(txn, rec);
    }
    /* free per-record temporaries (flux_put copied) */
    if (*meta) {
        for (uint32_t i = 0; i < *mc; ++i) { free((void*)(*meta)[i].key); free((void*)(*meta)[i].val); }
        free(*meta);
    }
    if (*links) {
        for (uint32_t i = 0; i < *lc; ++i) free((void*)(*links)[i].rel);
        free(*links);
    }
    free((void*)rec->kind); free((void*)rec->ptype); free((void*)rec->path);
    free((void*)rec->payload.data);
    memset(rec, 0, sizeof(*rec));
    *meta = NULL; *mc = 0; *links = NULL; *lc = 0;
}

flux_status_t flux_conv_from_fluxa(const char* in_fluxa, flux_txn_t* txn) {
    if (!in_fluxa || !txn) return FLUX_ERR_ARG;
    FILE* f = fopen(in_fluxa, "rb");
    if (!f) return FLUX_ERR_IO;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* src = (char*)malloc((size_t)sz + 1);
    if (!src) { fclose(f); return FLUX_ERR_NOMEM; }
    size_t total = fread(src, 1, (size_t)sz, f);
    src[total] = '\0';
    fclose(f);

    flux_record_t rec;
    memset(&rec, 0, sizeof(rec));
    flux_meta_kv_t* meta = NULL; uint32_t mc = 0;
    flux_link_t* links = NULL; uint32_t lc = 0;
    size_t off = 0;
    size_t llen = 0;
    char* line;

    while ((line = read_line(src, total, &off, &llen)) != NULL) {
        if (llen == 0 || line[0] == '#') { free(line); continue; }
        char tag = line[0];
        char* rest = (llen >= 2 && line[1] == ' ') ? line + 2 : line + 1;

        if (tag == 'R') {
            flush_rec(&rec, &meta, &mc, &links, &lc, txn);
            flux_id_from_hex(rest, &rec.id);
        } else if (tag == 'L') {
            rec.layer = (flux_layer_t)layer_from_str(rest);
        } else if (tag == 'K') {
            rec.kind = strdup(rest);
        } else if (tag == 'P') {
            rec.ptype = strdup(rest);
        } else if (tag == 'G') {
            rec.path = strdup(rest);
        } else if (tag == 'T') {
            rec.ts = strtoull(rest, NULL, 10);
        } else if (tag == 'C') {
            rec.clock = (flux_clock_t)clock_from_str(rest);
        } else if (tag == 'B') {
            rec.pclass = (strcmp(rest, "BIN") == 0) ? FLUX_PCLASS_BIN : FLUX_PCLASS_TEXT;
        } else if (tag == 'M') {
            char* eq = strchr(rest, '=');
            if (eq) {
                *eq = '\0';
                size_t kl = strlen(rest), vl = strlen(eq + 1);
                char* k = strdup(rest);
                char* v = strdup(eq + 1);
                kl = unescape_inplace(k, kl);
                vl = unescape_inplace(v, vl);
                k[kl] = '\0'; v[vl] = '\0';
                meta = realloc(meta, (mc + 1) * sizeof(flux_meta_kv_t));
                meta[mc].key = k; meta[mc].val = v; mc++;
            }
        } else if (tag == 'N') {
            char* eq = strchr(rest, '=');
            if (eq && eq - rest == 32) {
                *eq = '\0';
                flux_id_t tid;
                if (flux_id_from_hex(rest, &tid) == FLUX_OK) {
                    links = realloc(links, (lc + 1) * sizeof(flux_link_t));
                    memcpy(links[lc].target.bytes, tid.bytes, 16);
                    links[lc].rel = strdup(eq + 1);
                    lc++;
                }
            }
        } else if (tag == 'D') {
            size_t len = (size_t)strtoull(rest, NULL, 10);
            if (off + len <= total) {
                uint8_t* buf = (uint8_t*)malloc(len ? len : 1);
                memcpy(buf, src + off, len);
                rec.payload.data = buf;
                rec.payload.len = len;
                off += len;
                if (off < total && src[off] == '\n') off++; /* trailing newline */
            }
        } else if (tag == 'X') {
            size_t hexlen = (size_t)strtoull(rest, NULL, 10);
            parse_hex_payload(src, total, &off, hexlen, &rec);
        }
        free(line);
    }
    flush_rec(&rec, &meta, &mc, &links, &lc, txn);
    free(src);
    return FLUX_OK;
}
