/* Scan / filter iterator over the MVCC snapshot. See SPEC.md §6. */
#include "internal.h"
#include "fluxmeme/fluxmeme.h"
#include <stdlib.h>
#include <string.h>

struct flux_iter {
    flux_store_t* store;
    uint64_t snapshot;
    flux_filter_t filter;
    size_t idx;
};

flux_status_t flux_scan(const flux_txn_t* txn, const flux_filter_t* filter,
                        flux_iter_t** out) {
    if (!txn || !out) return FLUX_ERR_ARG;
    flux_iter_t* it = (flux_iter_t*)calloc(1, sizeof(flux_iter_t));
    if (!it) return FLUX_ERR_NOMEM;
    it->store = txn->store;
    it->snapshot = txn->snapshot;
    if (filter) it->filter = *filter;
    *out = it;
    return FLUX_OK;
}

static int record_matches(const flux_record_t* r, const flux_filter_t* f) {
    if (f->layer_mask && !(r->layer & f->layer_mask)) return 0;
    if (f->kind && (!r->kind || strcmp(r->kind, f->kind) != 0)) return 0;
    if (f->path_prefix && (!r->path || strncmp(r->path, f->path_prefix, strlen(f->path_prefix)) != 0))
        return 0;
    if (f->ts_lo && r->ts < f->ts_lo) return 0;
    if (f->ts_hi && r->ts > f->ts_hi) return 0;
    return 1;
}

flux_status_t flux_iter_next(flux_iter_t* it, flux_record_t* out) {
    if (!it || !out) return FLUX_ERR_ARG;
    flux_store_t* s = it->store;
    for (; it->idx < s->n_entries; ++it->idx) {
        const flux_entry_t* e = &s->entries[it->idx];
        if (e->ver > (uint32_t)it->snapshot) continue;
        /* is this entry THE live version at snapshot? */
        const flux_entry_t* live = flux_find_live(s, &e->id, it->snapshot);
        if (live != e) continue;          /* an older version of this id */
        if (e->is_tomb) continue;
        flux_status_t st = flux_store_read_entry(s, e->off, out);
        if (st != FLUX_OK) return st;
        it->idx++;
        if (!record_matches(out, &it->filter)) {
            flux_record_free(out);
            continue;                     /* live but filtered out */
        }
        return FLUX_OK;
    }
    return FLUX_ERR_NOTFOUND;
}

void flux_iter_free(flux_iter_t* it) {
    free(it);
}
