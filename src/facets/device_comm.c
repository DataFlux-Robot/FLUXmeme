/* device-comm facet — load + protocol query. */
#include "device_comm.h"
#include "../core/ref_utils.h"
#include <stdlib.h>
#include <string.h>

static const char* meta_val(const flux_record_t* r, const char* key) {
    for (uint32_t i = 0; i < r->meta_count; ++i)
        if (r->meta[i].key && strcmp(r->meta[i].key, key) == 0)
            return r->meta[i].val ? r->meta[i].val : "";
    return "";
}
static void copy_str(char* dst, size_t n, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

flux_status_t flux_dcomm_load(const flux_txn_t* txn, flux_dcomm* out) {
    if (!txn || !out) return FLUX_ERR_ARG;
    memset(out, 0, sizeof(*out));
    flux_iter_t* it = NULL;
    flux_status_t st = flux_scan(txn, NULL, &it);
    if (st != FLUX_OK) return st;
    flux_record_t r;
    while (flux_iter_next(it, &r) == FLUX_OK) {
        if (!(r.layer & FLUX_LAYER_BODY) || !r.kind) { flux_record_free(&r); continue; }
        if (strcmp(r.kind, "device-comm/node") == 0) {
            out->nodes = realloc(out->nodes, (out->n_nodes + 1) * sizeof(flux_dnode));
            flux_dnode* n = &out->nodes[out->n_nodes++];
            memset(n, 0, sizeof(*n));
            n->id = r.id;
            copy_str(n->name, sizeof(n->name), meta_val(&r, "name"));
            copy_str(n->kind, sizeof(n->kind), meta_val(&r, "kind"));
            copy_str(n->protocol, sizeof(n->protocol), meta_val(&r, "protocol"));
            copy_str(n->addr, sizeof(n->addr), meta_val(&r, "addr"));
            n->baud = strtoull(meta_val(&r, "baud"), NULL, 10);
        } else if (strcmp(r.kind, "device-comm/edge") == 0) {
            out->edges = realloc(out->edges, (out->n_edges + 1) * sizeof(flux_dedge));
            flux_dedge* e = &out->edges[out->n_edges++];
            memset(e, 0, sizeof(*e));
            e->id = r.id;
            copy_str(e->protocol, sizeof(e->protocol), meta_val(&r, "protocol"));
            /* endpoints a/b are REF-typed meta entries (keys "a"/"b") */
            char hex33[33]; const char* graph = NULL;
            if (flux_ref_by_rel(&r, "a", hex33, &graph) == FLUX_OK)
                flux_id_from_hex(hex33, &e->a);
            if (flux_ref_by_rel(&r, "b", hex33, &graph) == FLUX_OK)
                flux_id_from_hex(hex33, &e->b);
        }
        flux_record_free(&r);
    }
    flux_iter_free(it);
    return FLUX_OK;
}

void flux_dcomm_free(flux_dcomm* g) {
    if (!g) return;
    free(g->nodes);
    free(g->edges);
    memset(g, 0, sizeof(*g));
}

size_t flux_dcomm_edges_by_protocol(const flux_dcomm* g, const char* protocol,
                                    const flux_dedge** out) {
    if (!g || !protocol) return 0;
    static flux_dedge buf[256]; /* simple thread-local-ish; fine for slice */
    size_t n = 0;
    for (size_t i = 0; i < g->n_edges && n < 256; ++i) {
        if (strcmp(g->edges[i].protocol, protocol) == 0)
            buf[n++] = g->edges[i];
    }
    if (out) *out = buf;
    return n;
}
