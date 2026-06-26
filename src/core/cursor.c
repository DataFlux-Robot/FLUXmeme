/* FIFO cursor — independent per-consumer progress over the append-only log.
 * Each cursor remembers its position; multiple cursors consume the same log
 * independently (MIMO output side). See SPEC.md §6. */
#include "internal.h"
#include "fluxmeme/fluxmeme.h"
#include <stdlib.h>

struct flux_cursor {
    flux_store_t* store;
    size_t next_idx;   /* index into store->entries */
};

flux_status_t flux_cursor_open(flux_store_t* store, uint64_t start_seq,
                               flux_cursor_t** out) {
    if (!store || !out) return FLUX_ERR_ARG;
    flux_cursor_t* c = (flux_cursor_t*)calloc(1, sizeof(flux_cursor_t));
    if (!c) return FLUX_ERR_NOMEM;
    c->store = store;
    /* fast-forward to the first entry with ver >= start_seq */
    for (size_t i = 0; i < store->n_entries; ++i) {
        if (store->entries[i].ver >= (uint32_t)start_seq) { c->next_idx = i; break; }
        c->next_idx = i + 1;
    }
    *out = c;
    return FLUX_OK;
}

flux_status_t flux_cursor_next(flux_cursor_t* c, flux_record_t* out) {
    if (!c || !out) return FLUX_ERR_ARG;
    flux_store_t* s = c->store;
    for (; c->next_idx < s->n_entries; ++c->next_idx) {
        const flux_entry_t* e = &s->entries[c->next_idx];
        c->next_idx++;
        if (e->is_tomb) continue;            /* internal; skip for journal consumers */
        flux_status_t st = flux_store_read_entry(s, e->off, out);
        if (st != FLUX_OK) return st;
        return FLUX_OK;
    }
    return FLUX_ERR_NOTFOUND;
}

uint64_t flux_cursor_seq(const flux_cursor_t* c) {
    if (!c || c->next_idx == 0 || c->store->n_entries == 0) return 0;
    size_t i = c->next_idx - 1;
    if (i >= c->store->n_entries) i = c->store->n_entries - 1;
    return c->store->entries[i].ver;
}

void flux_cursor_free(flux_cursor_t* c) {
    free(c);
}
