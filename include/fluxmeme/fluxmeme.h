/* FLUXmeme — public API. See SPEC.md. */
#ifndef FLUXMEME_H
#define FLUXMEME_H

#include "types.h"
#include "backend.h"
#include "codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Version */
#define FLUXMEME_VERSION_MAJOR 0
#define FLUXMEME_VERSION_MINOR 1
#define FLUXMEME_VERSION_PATCH 0
const char* fluxmeme_version(void);

/* Thread-local last error string (set on non-OK status). */
const char* flux_last_error(void);

/* --- Lifecycle ------------------------------------------------------------ */
typedef struct flux_store flux_store_t;

/* Open or create a .flux store at `path`. writable=0 opens read-only snapshot. */
flux_status_t flux_open(const char* path, int writable, flux_store_t** out);
/* Open with a custom backend (mmap/flash/file). Takes ownership of backend. */
flux_status_t flux_open_with_backend(flux_backend_t* backend, flux_store_t** out);
void flux_close(flux_store_t* store);

/* Current visible commit_seq (MVCC version). */
uint64_t flux_commit_seq(const flux_store_t* store);

/* Reclaim append-only growth: rewrite `path` keeping only the live records
 * (drops tombstones, superseded versions, and any torn tail). Atomic rename. */
flux_status_t flux_compact(const char* path);

/* --- Transactions --------------------------------------------------------- */
typedef struct flux_txn flux_txn_t;

/* Read txn: lock-free snapshot at the current commit_seq. */
flux_status_t flux_txn_begin_read(flux_store_t* store, flux_txn_t** out);
/* Write txn: exclusive; serialized single writer; FIFO-ordered. */
flux_status_t flux_txn_begin_write(flux_store_t* store, flux_txn_t** out);
flux_status_t flux_txn_commit(flux_txn_t* txn);
flux_status_t flux_txn_rollback(flux_txn_t* txn);

/* --- CRUD ----------------------------------------------------------------- */
/* put: copies rec's fields into the store; fills rec->id (if zero) and
 * rec->ver. Returns FLUX_OK on success. */
flux_status_t flux_put(flux_txn_t* txn, flux_record_t* rec);
/* get: deep-copies record `id` at the txn snapshot into *out; free with
 * flux_record_free(). FLUX_ERR_NOTFOUND if absent/deleted. */
flux_status_t flux_get(flux_txn_t* txn, const flux_id_t* id, flux_record_t* out);
/* del: writes a tombstone for id at this commit. */
flux_status_t flux_del(flux_txn_t* txn, const flux_id_t* id);
/* cas: optimistic; commits only if id's current ver == expected_ver. */
flux_status_t flux_cas(flux_txn_t* txn, const flux_id_t* id, uint32_t expected_ver,
                       flux_record_t* newrec);

void flux_record_free(flux_record_t* rec); /* free heap copy from get/iter */

/* --- Scan / filter -------------------------------------------------------- */
typedef struct {
    uint32_t layer_mask;       /* 0 = any layer (bitset of FLUX_LAYER_*) */
    const char* kind;          /* NULL = any */
    const char* path_prefix;   /* NULL = any */
    uint64_t ts_lo, ts_hi;     /* 0 = unbounded on that side */
} flux_filter_t;

typedef struct flux_iter flux_iter_t;
flux_status_t flux_scan(const flux_txn_t* txn, const flux_filter_t* filter,
                        flux_iter_t** out);
/* Returns next record (heap copy, free with flux_record_free) or
 * FLUX_ERR_NOTFOUND when exhausted. */
flux_status_t flux_iter_next(flux_iter_t* it, flux_record_t* out);
void flux_iter_free(flux_iter_t* it);

/* --- FIFO cursors (multi-consumer, independent progress) ------------------ */
typedef struct flux_cursor flux_cursor_t;
flux_status_t flux_cursor_open(flux_store_t* store, uint64_t start_seq,
                               flux_cursor_t** out);
/* Next record with ver > cursor's last seen; FLUX_ERR_NOTFOUND at head. */
flux_status_t flux_cursor_next(flux_cursor_t* c, flux_record_t* out);
uint64_t flux_cursor_seq(const flux_cursor_t* c);
void flux_cursor_free(flux_cursor_t* c);

/* --- Transcoders (application layer; project the composed view) ----------- */
/* OKF: MIND/kind=concept <-> directory of markdown + frontmatter. */
flux_status_t flux_to_okf(const flux_txn_t* txn, const char* out_dir);
flux_status_t flux_from_okf(const char* in_dir, flux_txn_t* txn);
/* A2A: MIND/kind=agent_card|task <-> agent-card.json + tasks/ + artifacts/. */
flux_status_t flux_to_a2a(const flux_txn_t* txn, const char* out_dir);
flux_status_t flux_from_a2a(const char* in_dir, flux_txn_t* txn);
/* USD: BODY <-> a USDA file (mesh/xform; minimal round-trip, see SPEC §1.7). */
flux_status_t flux_to_usd(const flux_txn_t* txn, const char* out_usda);
flux_status_t flux_from_usd(const char* in_usda, flux_txn_t* txn);

/* .fluxa <-> .flux: text canonical source <-> binary store. `conv` round-trips
 * the whole store record-for-record (see SPEC §1.5). */
flux_status_t flux_conv_to_fluxa(const flux_txn_t* txn, const char* out_fluxa);
flux_status_t flux_conv_from_fluxa(const char* in_fluxa, flux_txn_t* txn);

/* MAVLink: JOURNAL/signal <-> a file of MAVLink v2 frames (FLUX_PARAM message).
 * Only signal/param records go on the bus; large assets are filtered out
 * (SPEC §6B). Pure codec — no link/scheduling (caller drives the bus). */
flux_status_t flux_to_mavlink(const flux_txn_t* txn, const char* out_frames);
flux_status_t flux_from_mavlink(const char* in_frames, flux_txn_t* txn);

/* Robot description import: URDF/SDF XML -> BODY robot-graph records
 * (robot/link + robot/joint with parent/child edges). URDF is a tree; cycles
 * (closed loops) can be added afterward via robot/constraint. */
flux_status_t flux_from_urdf(const char* in_urdf, flux_txn_t* txn);
flux_status_t flux_from_sdf(const char* in_sdf, flux_txn_t* txn);

/* MCAP (rosbag2): JOURNAL/signal <-> a .mcap file (the journal's primary
 * projection — a structured rosbag replacement carrying asset context). */
flux_status_t flux_to_mcap(const flux_txn_t* txn, const char* out_mcap);
flux_status_t flux_from_mcap(const char* in_mcap, flux_txn_t* txn);

#ifdef __cplusplus
}
#endif
#endif /* FLUXMEME_H */
