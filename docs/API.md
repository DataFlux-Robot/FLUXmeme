# C API Reference

Public header: [`include/fluxmeme/fluxmeme.h`](../include/fluxmeme/fluxmeme.h).
All functions return `flux_status_t` (`FLUX_OK` on success); `flux_last_error()`
gives a thread-local message. See [SPEC.md](../SPEC.md) for semantics.

## Lifecycle
```c
flux_status_t flux_open(const char* path, int writable, flux_store_t** out);
flux_status_t flux_open_with_backend(flux_backend_t* be, flux_store_t** out);
void          flux_close(flux_store_t* store);
uint64_t      flux_commit_seq(const flux_store_t* store);
```

## Transactions (MVCC)
```c
flux_status_t flux_txn_begin_read (flux_store_t* s, flux_txn_t** out); /* lock-free snapshot */
flux_status_t flux_txn_begin_write(flux_store_t* s, flux_txn_t** out); /* exclusive */
flux_status_t flux_txn_commit     (flux_txn_t* txn);
flux_status_t flux_txn_rollback   (flux_txn_t* txn);
```

## CRUD
```c
flux_status_t flux_put(flux_txn_t* txn, flux_record_t* rec); /* fills rec->id/ver */
flux_status_t flux_get(flux_txn_t* txn, const flux_id_t* id, flux_record_t* out);
flux_status_t flux_del(flux_txn_t* txn, const flux_id_t* id);            /* tombstone */
flux_status_t flux_cas(flux_txn_t* txn, const flux_id_t* id,             /* optimistic */
                       uint32_t expected_ver, flux_record_t* newrec);
void flux_record_free(flux_record_t* rec);  /* free a heap copy from get/iter */
```

## Scan / FIFO cursor
```c
typedef struct { uint32_t layer_mask; const char* kind; const char* path_prefix;
                 uint64_t ts_lo, ts_hi; } flux_filter_t;
flux_status_t flux_scan(const flux_txn_t* txn, const flux_filter_t* f, flux_iter_t** out);
flux_status_t flux_iter_next(flux_iter_t* it, flux_record_t* out); /* FLUX_ERR_NOTFOUND at end */
void          flux_iter_free(flux_iter_t* it);

flux_status_t flux_cursor_open(flux_store_t* s, uint64_t start_seq, flux_cursor_t** out);
flux_status_t flux_cursor_next(flux_cursor_t* c, flux_record_t* out); /* independent progress */
void          flux_cursor_free(flux_cursor_t* c);
```

## Transcoders (application-layer projections)
```c
flux_status_t flux_to_okf  (const flux_txn_t* txn, const char* out_dir);
flux_status_t flux_from_okf(const char* in_dir, flux_txn_t* txn);
flux_status_t flux_to_a2a  (const flux_txn_t* txn, const char* out_dir);
flux_status_t flux_from_a2a(const char* in_dir, flux_txn_t* txn);
flux_status_t flux_to_usd  (const flux_txn_t* txn, const char* out_usda);
flux_status_t flux_from_usd(const char* in_usda, flux_txn_t* txn);
flux_status_t flux_to_mavlink  (const flux_txn_t* txn, const char* out_frames);
flux_status_t flux_from_mavlink(const char* in_frames, flux_txn_t* txn);
flux_status_t flux_conv_to_fluxa  (const flux_txn_t* txn, const char* out_fluxa);
flux_status_t flux_conv_from_fluxa(const char* in_fluxa, flux_txn_t* txn);
flux_status_t flux_from_urdf(const char* in_urdf, flux_txn_t* txn);
flux_status_t flux_from_sdf (const char* in_sdf,  flux_txn_t* txn);
```

## Composition (LIVRPS, see `src/compose/compose.h`)
```c
flux_status_t flux_compose_open(const char* root_path, flux_compose_t** out);
flux_status_t flux_compose_set_variant(flux_compose_t* c, const char* set, const char* value);
flux_status_t flux_compose_get(flux_compose_t* c, const flux_id_t* id, flux_record_t* out);
```

## Record (the universal atom)
```c
typedef struct {
    flux_id_t id; flux_layer_t layer; flux_pclass_t pclass; flux_clock_t clock;
    const char* path, *ptype, *kind;
    const flux_meta_kv_t* meta; uint32_t meta_count;
    const flux_link_t*    links; uint32_t link_count;
    flux_buf_t payload; uint64_t ts; uint32_t ver;
} flux_record_t;
```

**Ownership:** input pointers to `flux_put` are borrowed for the call (the engine
copies). Records returned from `flux_get`/`flux_iter_next` are heap copies owned
by the caller — free with `flux_record_free`. See `demo/` for end-to-end usage.
