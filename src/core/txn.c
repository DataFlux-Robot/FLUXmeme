/* Transactions: lock-free read snapshots + serialized single writer + MVCC.
 * See SPEC.md §6. */
#include "internal.h"
#include "fluxmeme/fluxmeme.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

flux_status_t flux_txn_begin_read(flux_store_t* store, flux_txn_t** out) {
    flux_txn_t* t = (flux_txn_t*)calloc(1, sizeof(flux_txn_t));
    if (!t) return FLUX_ERR_NOMEM;
    t->store = store;
    t->readonly = 1;
    t->snapshot = store->commit_seq; /* lock-free snapshot */
    *out = t;
    return FLUX_OK;
}

flux_status_t flux_txn_begin_write(flux_store_t* store, flux_txn_t** out) {
    if (!store->writable) {
        flux_set_error("store opened read-only");
        return FLUX_ERR_LOCKED;
    }
    flux_status_t st = store->be->lock(store->be->self, 1);
    if (st != FLUX_OK) {
        flux_set_error("could not acquire write lock");
        return FLUX_ERR_LOCKED;
    }
    flux_txn_t* t = (flux_txn_t*)calloc(1, sizeof(flux_txn_t));
    if (!t) { store->be->unlock(store->be->self); return FLUX_ERR_NOMEM; }
    t->store = store;
    t->readonly = 0;
    t->snapshot = store->commit_seq + 1; /* target ver for new records */
    *out = t;
    return FLUX_OK;
}

flux_status_t flux_txn_commit(flux_txn_t* txn) {
    if (!txn) return FLUX_ERR_ARG;
    if (txn->readonly) { free(txn); return FLUX_OK; }
    if (txn->committed) { free(txn); return FLUX_OK; }

    flux_store_t* s = txn->store;
    uint64_t target_seq = txn->snapshot;

    /* append all staged entries, capturing their body offsets */
    uint64_t* offs = NULL;
    if (txn->n_staged) {
        offs = (uint64_t*)malloc(txn->n_staged * sizeof(uint64_t));
        if (!offs) return FLUX_ERR_NOMEM;
    }
    for (size_t i = 0; i < txn->n_staged; ++i) {
        flux_status_t st = flux_store_append_entry(s, &txn->staged[i].body, &offs[i]);
        if (st != FLUX_OK) {
            free(offs);
            s->be->unlock(s->be->self);
            return st;
        }
    }
    /* commit marker (makes them visible atomically) */
    uint64_t ts = (uint64_t)time(NULL) * 1000ULL;
    flux_status_t cst = flux_store_append_commit(s, target_seq, ts, (uint32_t)txn->n_staged);
    if (cst != FLUX_OK) {
        free(offs);
        s->be->unlock(s->be->self);
        return cst;
    }
    /* promote staged into the in-memory index */
    for (size_t i = 0; i < txn->n_staged; ++i) {
        if (s->n_entries == s->cap_entries) {
            s->cap_entries = s->cap_entries ? s->cap_entries * 2 : 64;
            s->entries = realloc(s->entries, s->cap_entries * sizeof(flux_entry_t));
            if (!s->entries) { free(offs); return FLUX_ERR_NOMEM; }
        }
        flux_entry_t* e = &s->entries[s->n_entries++];
        memcpy(e->id.bytes, txn->staged[i].id.bytes, 16);
        e->off = offs[i];
        e->body_len = (uint32_t)txn->staged[i].body.len;
        e->ver = (uint32_t)target_seq;
        e->layer = txn->staged[i].layer;
        e->is_tomb = (uint8_t)txn->staged[i].is_tomb;
    }
    s->commit_seq = target_seq;
    txn->committed = 1;
    free(offs);

    /* free staged bodies */
    for (size_t i = 0; i < txn->n_staged; ++i)
        free((void*)txn->staged[i].body.data);
    free(txn->staged);
    s->be->unlock(s->be->self);
    free(txn);
    return FLUX_OK;
}

flux_status_t flux_txn_rollback(flux_txn_t* txn) {
    if (!txn) return FLUX_ERR_ARG;
    for (size_t i = 0; i < txn->n_staged; ++i)
        free((void*)txn->staged[i].body.data);
    free(txn->staged);
    if (!txn->readonly)
        txn->store->be->unlock(txn->store->be->self);
    free(txn);
    return FLUX_OK;
}

/* ---- find the live version of id at snapshot S ---- */
static const flux_entry_t* find_live(const flux_store_t* s, const flux_id_t* id,
                                     uint64_t snapshot) {
    const flux_entry_t* best = NULL;
    for (size_t i = 0; i < s->n_entries; ++i) {
        const flux_entry_t* e = &s->entries[i];
        if (memcmp(e->id.bytes, id->bytes, 16) != 0) continue;
        if (e->ver > snapshot) continue;
        if (!best || e->ver >= best->ver) best = e; /* ties: latest put wins */
    }
    return best;
}

flux_status_t flux_get(flux_txn_t* txn, const flux_id_t* id, flux_record_t* out) {
    if (!txn || !id || !out) return FLUX_ERR_ARG;
    const flux_entry_t* e = find_live(txn->store, id, txn->snapshot);
    if (!e || e->is_tomb) return FLUX_ERR_NOTFOUND;
    return flux_store_read_entry(txn->store, e->off, out);
}

flux_status_t flux_put(flux_txn_t* txn, flux_record_t* rec) {
    if (!txn || !rec) return FLUX_ERR_ARG;
    if (txn->readonly) { flux_set_error("read-only txn"); return FLUX_ERR_LOCKED; }
    if (flux_id_is_zero(&rec->id))
        flux_id_gen(&rec->id);
    rec->ver = (uint32_t)txn->snapshot;
    rec->ts = rec->ts ? rec->ts : (uint64_t)time(NULL) * 1000ULL;
    flux_buf_t body;
    flux_status_t st = flux_record_encode(rec, &body);
    if (st != FLUX_OK) return st;
    if (txn->n_staged == txn->cap_staged) {
        txn->cap_staged = txn->cap_staged ? txn->cap_staged * 2 : 16;
        txn->staged = realloc(txn->staged, txn->cap_staged * sizeof(flux_staged_t));
        if (!txn->staged) return FLUX_ERR_NOMEM;
    }
    flux_staged_t* stg = &txn->staged[txn->n_staged++];
    stg->body = body;
    memcpy(stg->id.bytes, rec->id.bytes, 16);
    stg->layer = (uint8_t)rec->layer;
    stg->is_tomb = 0;
    return FLUX_OK;
}

flux_status_t flux_del(flux_txn_t* txn, const flux_id_t* id) {
    if (!txn || !id) return FLUX_ERR_ARG;
    if (txn->readonly) return FLUX_ERR_LOCKED;
    char hex[33];
    flux_id_to_hex(id, hex);
    const char* k = "target";
    flux_meta_kv_t meta = { k, hex, FLUX_META_STRING };
    flux_record_t rec;
    memset(&rec, 0, sizeof(rec));
    memcpy(rec.id.bytes, id->bytes, 16);
    rec.layer = (flux_layer_t)0; /* tombstone sentinel */
    rec.kind = "__tomb__";
    rec.meta = &meta;
    rec.meta_count = 1;
    rec.ver = (uint32_t)txn->snapshot;
    rec.ts = (uint64_t)time(NULL) * 1000ULL;
    flux_buf_t body;
    flux_status_t st = flux_record_encode(&rec, &body);
    if (st != FLUX_OK) return st;
    if (txn->n_staged == txn->cap_staged) {
        txn->cap_staged = txn->cap_staged ? txn->cap_staged * 2 : 16;
        txn->staged = realloc(txn->staged, txn->cap_staged * sizeof(flux_staged_t));
        if (!txn->staged) return FLUX_ERR_NOMEM;
    }
    flux_staged_t* stg = &txn->staged[txn->n_staged++];
    stg->body = body;
    memcpy(stg->id.bytes, id->bytes, 16);
    stg->layer = 0;
    stg->is_tomb = 1;
    return FLUX_OK;
}

flux_status_t flux_cas(flux_txn_t* txn, const flux_id_t* id, uint32_t expected_ver,
                       flux_record_t* newrec) {
    if (!txn || !id) return FLUX_ERR_ARG;
    if (txn->readonly) return FLUX_ERR_LOCKED;
    const flux_entry_t* e = find_live(txn->store, id, txn->store->commit_seq);
    uint32_t cur = e ? e->ver : 0;
    if (cur != expected_ver) {
        flux_set_error("CAS mismatch (cur=%u expected=%u)", cur, expected_ver);
        return FLUX_ERR_VERSION;
    }
    /* keep the same id; put new version */
    memcpy(newrec->id.bytes, id->bytes, 16);
    return flux_put(txn, newrec);
}

/* exposed for iter.c / cursor.c */
const flux_entry_t* flux_find_live(const flux_store_t* s, const flux_id_t* id,
                                   uint64_t snapshot) {
    return find_live(s, id, snapshot);
}
