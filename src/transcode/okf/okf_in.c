/* OKF in: ingest a markdown bundle (concepts/*.md) into the store as
 * MIND/kind=concept records. Reverses okf_out. See SPEC.md §2. */
#include "fluxmeme/fluxmeme.h"
#include "../fsutil.h"
#include "../../core/ref_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static char* read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    *out_len = rd;
    return buf;
}

static int has_suffix(const char* s, const char* suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

/* collector: each .md name -> we ingest it. */
typedef struct { const char* dir; flux_txn_t* txn; int count; } ingest_ctx;

static void ingest_one(const char* name, void* ud) {
    ingest_ctx* c = (ingest_ctx*)ud;
    if (!has_suffix(name, ".md")) return;
    /* id from filename (hex) */
    flux_id_t id;
    if (flux_id_from_hex(name, &id) != FLUX_OK) return; /* skip index/log/etc. */

    char path[1100];
    snprintf(path, sizeof(path), "%s%c%s", c->dir, FLUX_PATH_SEP, name);
    size_t len = 0;
    char* content = read_file(path, &len);
    if (!content) return;

    /* parse frontmatter (between leading "---" lines) */
    char* p = content;
    char* body = content;
    flux_meta_kv_t meta[32];
    uint32_t mc = 0;
    const char* kind = "concept";
    if (strncmp(p, "---\n", 4) == 0) {
        p += 4;
        char* line_end = strchr(p, '\n');
        while (line_end) {
            *line_end = '\0';
            if (strcmp(p, "---") == 0) { body = line_end + 1; break; }
            char* colon = strchr(p, ':');
            if (colon && mc < 32) {
                *colon = '\0';
                char* v = colon + 1;
                while (*v == ' ') v++;
                meta[mc].key = strdup(p);
                meta[mc].val = strdup(v);
                meta[mc].type = FLUX_META_STRING;
                if (strcmp(p, "type") == 0) kind = meta[mc].val;
                mc++;
            }
            p = line_end + 1;
            line_end = strchr(p, '\n');
        }
    }
    /* trim leading blank line in body */
    if (*body == '\n') body++;
    size_t body_len = strlen(body);

    /* parse the "## Links" wikilink section (okf_out convention); strip it from
     * the body so the markdown payload round-trips cleanly. Each link becomes a
     * REF-typed meta entry: key=rel, val="32hex" (encoded via flux_ref_encode). */
    char* lsec = strstr(body, "\n## Links");
    if (lsec) {
        body_len = (size_t)(lsec - body);
        char* line = strchr(lsec, '\n');
        while (line && mc < 32) {
            line++;
            if (strncmp(line, "- [[", 4) != 0) { line = strchr(line, '\n'); continue; }
            char* hexp = line + 4;
            char* close = strstr(hexp, "]]");
            if (!close || close - hexp != 32) { line = strchr(line, '\n'); continue; }
            char hexbuf[33];
            memcpy(hexbuf, hexp, 32);
            hexbuf[32] = '\0';
            flux_id_t tid;
            if (flux_id_from_hex(hexbuf, &tid) != FLUX_OK) { line = strchr(line, '\n'); continue; }
            /* rel key (default "related"); pull from trailing "(...)" */
            char relbuf[64];
            const char* rel = "related";
            char* paren = strchr(close, '(');
            char* pend = paren ? strchr(paren, ')') : NULL;
            if (paren && pend && pend > paren + 1) {
                size_t rl = (size_t)(pend - paren - 1);
                if (rl >= sizeof(relbuf)) rl = sizeof(relbuf) - 1;
                memcpy(relbuf, paren + 1, rl);
                relbuf[rl] = '\0';
                rel = relbuf;
            }
            char refval[33];
            flux_ref_encode(refval, sizeof(refval), hexbuf, "");
            meta[mc].key = strdup(rel);
            meta[mc].val = strdup(refval);
            meta[mc].type = FLUX_META_REF;
            mc++;
            line = strchr(line, '\n');
        }
    }
    while (body_len && (body[body_len - 1] == '\n' || body[body_len - 1] == '\r')) body_len--;

    flux_record_t rec;
    memset(&rec, 0, sizeof(rec));
    memcpy(rec.id.bytes, id.bytes, 16);
    rec.layer = FLUX_LAYER_MIND;
    rec.pclass = FLUX_PCLASS_TEXT;
    rec.kind = kind;
    rec.ptype = "text/markdown";
    rec.meta = meta;
    rec.meta_count = mc;
    rec.payload.data = (const uint8_t*)body;
    rec.payload.len = body_len;
    rec.ts = (uint64_t)time(NULL) * 1000ULL;

    flux_put(c->txn, &rec);

    for (uint32_t i = 0; i < mc; ++i) { free((void*)meta[i].key); free((void*)meta[i].val); }
    free(content);
    c->count++;
}

flux_status_t flux_from_okf(const char* in_dir, flux_txn_t* txn) {
    if (!in_dir || !txn) return FLUX_ERR_ARG;
    char concepts[1024];
    snprintf(concepts, sizeof(concepts), "%s%cconcepts", in_dir, FLUX_PATH_SEP);
    ingest_ctx c = { concepts, txn, 0 };
    if (flux_list_dir(concepts, ingest_one, &c) < 0) return FLUX_ERR_IO;
    return FLUX_OK;
}
