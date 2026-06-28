/* v2 REF helpers — scan and create REF-typed meta entries.
 * These replace the deleted links[] API. */
#ifndef FLUX_REF_UTILS_H
#define FLUX_REF_UTILS_H
#include "internal.h"
#include <string.h>
#include <stdlib.h>

/* count REF meta entries on a record */
static inline uint32_t flux_ref_count(const flux_record_t* r) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < r->meta_count; ++i)
        if (r->meta[i].type == FLUX_META_REF) ++n;
    return n;
}

/* iterate REF meta: call with index 0,1,2... until FLUX_ERR_NOTFOUND.
 * returns the target id (hex in out_hex[33]), rel (key), graph (from @suffix) */
static inline flux_status_t flux_ref_at(const flux_record_t* r, uint32_t idx,
                                         char* out_hex33, const char** out_rel,
                                         const char** out_graph) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < r->meta_count; ++i) {
        if (r->meta[i].type != FLUX_META_REF) continue;
        if (n == idx) {
            const char* val = r->meta[i].val ? r->meta[i].val : "";
            const char* at = strchr(val, '@');
            size_t hlen = at ? (size_t)(at - val) : strlen(val);
            if (hlen >= 32) {
                memcpy(out_hex33, val, 32);
                out_hex33[32] = '\0';
            } else { out_hex33[0] = '\0'; }
            if (out_rel)   *out_rel   = r->meta[i].key;
            if (out_graph) *out_graph = at ? at + 1 : "";
            return FLUX_OK;
        }
        ++n;
    }
    return FLUX_ERR_NOTFOUND;
}

/* find a REF by rel key (e.g. "parent"). Returns first match. */
static inline flux_status_t flux_ref_by_rel(const flux_record_t* r, const char* rel,
                                              char* out_hex33, const char** out_graph) {
    for (uint32_t i = 0; i < r->meta_count; ++i) {
        if (r->meta[i].type != FLUX_META_REF) continue;
        if (r->meta[i].key && strcmp(r->meta[i].key, rel) == 0) {
            const char* val = r->meta[i].val ? r->meta[i].val : "";
            const char* at = strchr(val, '@');
            size_t hlen = at ? (size_t)(at - val) : strlen(val);
            if (hlen >= 32) { memcpy(out_hex33, val, 32); out_hex33[32] = '\0'; }
            else out_hex33[0] = '\0';
            if (out_graph) *out_graph = at ? at + 1 : "";
            return FLUX_OK;
        }
    }
    return FLUX_ERR_NOTFOUND;
}

/* encode a REF value string: "32hex@graph" or "32hex" */
static inline void flux_ref_encode(char* buf, size_t bufsz,
                                    const char* hex32, const char* graph) {
    if (graph && *graph)
        snprintf(buf, bufsz, "%s@%s", hex32, graph);
    else
        snprintf(buf, bufsz, "%s", hex32);
}

#endif /* FLUX_REF_UTILS_H */
