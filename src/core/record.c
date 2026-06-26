/* Record wire codec — encode/decode the RecordEntry body (SPEC.md §2, §3).
 *
 * Body layout (little-endian, length-prefixed):
 *   id[16] layer[1] pclass[1] clock[1] rsv[1] ts[8] ver[4] content_hash[32]
 *   path_len[2] path[]
 *   ptype_len[2] ptype[]
 *   kind_len[2]  kind[]
 *   meta_count[2]  meta_count*( k_len[2] k[] v_len[2] v[] )
 *   link_count[2]  link_count*( target_id[16] rel_len[2] rel[] )
 *   payload_len[4] payload[]
 *
 * content_hash = flux_hash over body excluding the 16-byte id (i.e. the slot is
 * zeroed during hashing). Two records with identical content (bar id) hash alike.
 */
#include "internal.h"
#include <stdlib.h>
#include <string.h>

static uint16_t u16min(size_t v) { return (uint16_t)(v > 0xffff ? 0xffff : v); }

/* ---- encode: returns malloc'd buffer; caller frees out->data ---- */
flux_status_t flux_record_encode(const flux_record_t* rec, flux_buf_t* out) {
    if (!rec || !out)
        return FLUX_ERR_ARG;

    size_t path_len = rec->path ? strlen(rec->path) : 0;
    size_t ptype_len = rec->ptype ? strlen(rec->ptype) : 0;
    size_t kind_len = rec->kind ? strlen(rec->kind) : 0;
    size_t meta_sz = 0;
    for (uint32_t i = 0; i < rec->meta_count; ++i) {
        const char* k = rec->meta[i].key ? rec->meta[i].key : "";
        const char* v = rec->meta[i].val ? rec->meta[i].val : "";
        meta_sz += 4 + strlen(k) + strlen(v);
    }
    size_t link_sz = 0;
    for (uint32_t i = 0; i < rec->link_count; ++i) {
        const char* rel = rec->links[i].rel ? rec->links[i].rel : "";
        link_sz += 16 + 2 + strlen(rel);
    }
    size_t total = 16 + 4 + 8 + 4 + 32 + (2 + path_len) + (2 + ptype_len) +
                   (2 + kind_len) + (2 + meta_sz) + (2 + link_sz) + (4 + rec->payload.len);

    uint8_t* buf = (uint8_t*)malloc(total);
    if (!buf)
        return FLUX_ERR_NOMEM;
    memset(buf, 0, total);
    size_t off = 0;

    memcpy(buf + off, rec->id.bytes, 16);
    off += 16;
    buf[off++] = (uint8_t)rec->layer;
    buf[off++] = (uint8_t)rec->pclass;
    buf[off++] = (uint8_t)rec->clock;
    buf[off++] = 0; /* reserved */
    put_u64le(buf + off, rec->ts);
    off += 8;
    put_u32le(buf + off, rec->ver);
    off += 4;
    size_t hash_slot = off;
    off += 32; /* content_hash filled below */
    put_u16le(buf + off, u16min(path_len));
    off += 2;
    if (path_len) { memcpy(buf + off, rec->path, path_len); off += path_len; }
    put_u16le(buf + off, u16min(ptype_len));
    off += 2;
    if (ptype_len) { memcpy(buf + off, rec->ptype, ptype_len); off += ptype_len; }
    put_u16le(buf + off, u16min(kind_len));
    off += 2;
    if (kind_len) { memcpy(buf + off, rec->kind, kind_len); off += kind_len; }
    put_u16le(buf + off, u16min(rec->meta_count));
    off += 2;
    for (uint32_t i = 0; i < rec->meta_count; ++i) {
        const char* k = rec->meta[i].key ? rec->meta[i].key : "";
        const char* v = rec->meta[i].val ? rec->meta[i].val : "";
        size_t kl = strlen(k), vl = strlen(v);
        put_u16le(buf + off, u16min(kl)); off += 2;
        memcpy(buf + off, k, kl); off += kl;
        put_u16le(buf + off, u16min(vl)); off += 2;
        memcpy(buf + off, v, vl); off += vl;
    }
    put_u16le(buf + off, u16min(rec->link_count));
    off += 2;
    for (uint32_t i = 0; i < rec->link_count; ++i) {
        memcpy(buf + off, rec->links[i].target.bytes, 16); off += 16;
        const char* rel = rec->links[i].rel ? rec->links[i].rel : "";
        size_t rl = strlen(rel);
        put_u16le(buf + off, u16min(rl)); off += 2;
        memcpy(buf + off, rel, rl); off += rl;
    }
    put_u32le(buf + off, (uint32_t)rec->payload.len);
    off += 4;
    if (rec->payload.len) { memcpy(buf + off, rec->payload.data, rec->payload.len); off += rec->payload.len; }

    /* content_hash over body excluding id (slot already zeroed) */
    flux_hash(buf + 16, total - 16, buf + hash_slot);

    out->data = buf;
    out->len = total;
    return FLUX_OK;
}

/* ---- helpers for decode ---- */
static char* dupn(const uint8_t* src, size_t n) {
    char* s = (char*)malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, src, n);
    s[n] = '\0';
    return s;
}

/* ---- decode: heap-copy into out; free with flux_record_free ---- */
flux_status_t flux_record_decode(const uint8_t* body, size_t len, flux_record_t* out) {
    if (!body || !out)
        return FLUX_ERR_ARG;
    memset(out, 0, sizeof(*out));
    size_t off = 0;
    if (len < 16 + 4 + 8 + 4 + 32)
        return FLUX_ERR_CORRUPT;
    memcpy(out->id.bytes, body + off, 16); off += 16;
    out->layer = (flux_layer_t)body[off++];
    out->pclass = (flux_pclass_t)body[off++];
    out->clock = (flux_clock_t)body[off++];
    off++; /* reserved */
    out->ts = get_u64le(body + off); off += 8;
    out->ver = get_u32le(body + off); off += 4;
    /* content_hash[32] — skipped on decode (verify optionally) */
    off += 32;

#define READ_STR(field)                                                       \
    do {                                                                      \
        if (off + 2 > len) goto corrupt;                                      \
        uint16_t n = get_u16le(body + off); off += 2;                          \
        if (off + n > len) goto corrupt;                                      \
        (field) = n ? dupn(body + off, n) : NULL;                             \
        if (n && !(field)) return FLUX_ERR_NOMEM;                             \
        off += n;                                                             \
    } while (0)

    READ_STR(out->path);
    READ_STR(out->ptype);
    READ_STR(out->kind);

    if (off + 2 > len) goto corrupt;
    uint16_t mc = get_u16le(body + off); off += 2;
    out->meta_count = mc;
    flux_meta_kv_t* marr = NULL;
    if (mc) {
        marr = (flux_meta_kv_t*)calloc(mc, sizeof(flux_meta_kv_t));
        if (!marr) return FLUX_ERR_NOMEM;
    }
    for (uint16_t i = 0; i < mc; ++i) {
        char* k = NULL; char* v = NULL;
        READ_STR(k);
        READ_STR(v);
        marr[i].key = k;
        marr[i].val = v;
    }
    out->meta = marr;

    if (off + 2 > len) goto corrupt;
    uint16_t lc = get_u16le(body + off); off += 2;
    out->link_count = lc;
    flux_link_t* larr = NULL;
    if (lc) {
        larr = (flux_link_t*)calloc(lc, sizeof(flux_link_t));
        if (!larr) return FLUX_ERR_NOMEM;
    }
    for (uint16_t i = 0; i < lc; ++i) {
        if (off + 16 > len) goto corrupt;
        memcpy(larr[i].target.bytes, body + off, 16); off += 16;
        char* rel = NULL;
        READ_STR(rel);
        larr[i].rel = rel;
    }
    out->links = larr;

    if (off + 4 > len) goto corrupt;
    uint32_t plen = get_u32le(body + off); off += 4;
    if (off + plen > len) goto corrupt;
    if (plen) {
        uint8_t* pd = (uint8_t*)malloc(plen);
        if (!pd) return FLUX_ERR_NOMEM;
        memcpy(pd, body + off, plen);
        out->payload.data = pd;
        out->payload.len = plen;
        off += plen;
    }
#undef READ_STR
    return FLUX_OK;

corrupt:
    flux_record_free(out);
    return FLUX_ERR_CORRUPT;
}

void flux_record_free(flux_record_t* rec) {
    if (!rec) return;
    free((void*)rec->path);
    free((void*)rec->ptype);
    free((void*)rec->kind);
    if (rec->meta) {
        for (uint32_t i = 0; i < rec->meta_count; ++i) {
            free((void*)rec->meta[i].key);
            free((void*)rec->meta[i].val);
        }
        free((void*)rec->meta);
    }
    if (rec->links) {
        for (uint32_t i = 0; i < rec->link_count; ++i)
            free((void*)rec->links[i].rel);
        free((void*)rec->links);
    }
    free((void*)rec->payload.data);
    memset(rec, 0, sizeof(*rec));
}
