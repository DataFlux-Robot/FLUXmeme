/* FLUXmeme — core engine internals (not public). */
#ifndef FLUXMEME_INTERNAL_H
#define FLUXMEME_INTERNAL_H

#include "fluxmeme/fluxmeme.h" /* brings in flux_store_t/flux_txn_t typedefs */
#include <stdio.h>

/* ---- sentinels / magic ---- */
#define FLUX_MAGIC_FILE "FLXM"
#define FLUX_MAGIC_RECORD "RECD"
#define FLUX_MAGIC_COMMIT "COMT"
#define FLUX_MAGIC_FOOTER "FLXF"
#define FLUX_FMT_VERSION 2u
#define FLUX_HEADER_SIZE 64

/* untrusted-input hardening caps (SPEC §8) */
#define FLUX_MAX_PAYLOAD  (1u << 28)   /* 256 MiB per record payload            */
#define FLUX_MAX_META     4096u        /* KV pairs per record (v2: includes REF connections) */
#define FLUX_MAX_RECORDS  (1u << 24)   /* 16M entries cap on open (DoS bound)   */
#define FLUX_MAX_LAYERS   64           /* composition layer-stack depth (cycle) */

/* ---- error helper ---- */
void flux_set_error(const char* fmt, ...);

/* ---- crc32c (Castagnoli) ---- */
uint32_t flux_crc32c(const void* data, size_t len);
uint32_t flux_crc32c_continue(uint32_t seed, const void* data, size_t len);

/* ---- content hash (blake3-compatible 32-byte interface) ---- */
/* NOTE: v1.0 slice uses a compact deterministic hash; drops to vendored blake3
 * later under the same signature. */
void flux_hash(const void* data, size_t len, uint8_t out[32]);

/* ---- ULID-like id ---- */
void flux_id_gen(flux_id_t* out); /* time-first (sorted) + random */

/* ---- tiny endianness-safe byte helpers ---- */
static inline void put_u16le(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
}
static inline void put_u32le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}
static inline void put_u64le(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; ++i)
        p[i] = (uint8_t)((v >> (8 * i)) & 0xff);
}
static inline uint16_t get_u16le(const uint8_t* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t get_u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
static inline uint64_t get_u64le(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= ((uint64_t)p[i]) << (8 * i);
    return v;
}

/* ---- record codec (record.c) ---- */
flux_status_t flux_record_encode(const flux_record_t* rec, flux_buf_t* out);
flux_status_t flux_record_decode(const uint8_t* body, size_t len, flux_record_t* out);

static inline int flux_id_is_zero(const flux_id_t* id) {
    if (!id) return 1;
    for (int i = 0; i < 16; ++i)
        if (id->bytes[i]) return 0;
    return 1;
}

/* ---- engine structs ---- */
typedef struct {
    flux_id_t id;
    uint64_t off;      /* offset of the RecordEntry body in the file */
    uint32_t body_len;
    uint32_t ver;
    uint8_t layer;
    uint8_t is_tomb;
} flux_entry_t;

struct flux_store {
    flux_backend_t* be;
    int owns_be;
    int writable;
    uint64_t commit_seq;
    uint64_t create_ts;
    flux_entry_t* entries;
    size_t n_entries, cap_entries;
};

/* staged write in a write txn: encoded body + resolved id + tombstone flag */
typedef struct {
    flux_buf_t body;     /* encoded RecordEntry body */
    flux_id_t id;
    uint8_t layer;
    int is_tomb;
} flux_staged_t;

struct flux_txn {
    flux_store_t* store;
    int readonly;
    uint64_t snapshot;     /* read snapshot seq; write: target seq = store->commit_seq+1 */
    flux_staged_t* staged;
    size_t n_staged, cap_staged;
    int committed;
};

/* ---- engine helpers (store.c) ---- */
/* Append a raw entry (magic + body_len + body + pad + crc). Returns its body
 * offset. Used by txn commit. */
flux_status_t flux_store_append_entry(flux_store_t* s, const flux_buf_t* body,
                                      uint64_t* body_off_out);
flux_status_t flux_store_append_commit(flux_store_t* s, uint64_t seq,
                                       uint64_t ts, uint32_t delta);
/* Read body bytes at off into a freshly decoded record. */
flux_status_t flux_store_read_entry(flux_store_t* s, uint64_t off,
                                    flux_record_t* out);

/* resolve live version of id at snapshot (txn.c) */
const flux_entry_t* flux_find_live(const flux_store_t* s, const flux_id_t* id,
                                   uint64_t snapshot);

#endif /* FLUXMEME_INTERNAL_H */
