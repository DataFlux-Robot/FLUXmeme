/* FLUXmeme — core types. See SPEC.md. */
#ifndef FLUXMEME_TYPES_H
#define FLUXMEME_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Layers (bitset; the three "natures" of an embodied node) --- */
typedef enum {
    FLUX_LAYER_BODY    = 0x01, /* physical structure -> USD */
    FLUX_LAYER_MIND    = 0x02, /* knowledge + agent skills -> OKF + A2A */
    FLUX_LAYER_JOURNAL = 0x04  /* lifetime log / PHM -> MCAP + MAVLink */
} flux_layer_t;

/* --- Payload class --- */
typedef enum {
    FLUX_PCLASS_TEXT = 1, /* UTF-8 bytes */
    FLUX_PCLASS_BIN  = 2  /* raw bytes */
} flux_pclass_t;

/* --- Clock domain (JOURNAL records; multi-clock alignment across sim/real/MCU) --- */
typedef enum {
    FLUX_CLOCK_SIM_TIME        = 0,
    FLUX_CLOCK_WALL_TIME       = 1,
    FLUX_CLOCK_DEVICE_MONOTONIC = 2
} flux_clock_t;

/* --- Status --- */
typedef enum {
    FLUX_OK = 0,
    FLUX_ERR_IO,
    FLUX_ERR_CRC,
    FLUX_ERR_LOCKED,
    FLUX_ERR_NOTFOUND,
    FLUX_ERR_ARG,
    FLUX_ERR_VERSION, /* optimistic CAS mismatch */
    FLUX_ERR_NOMEM,
    FLUX_ERR_RANGE,   /* cap exceeded (untrusted-input hardening) */
    FLUX_ERR_CORRUPT
} flux_status_t;

/* 16-byte ULID-like identity. Sorted (time-first) + globally unique. */
typedef struct {
    uint8_t bytes[16];
} flux_id_t;

/* Non-owning byte view. */
typedef struct {
    const uint8_t* data;
    size_t len;
} flux_buf_t;

/* OKF-style frontmatter key/value pair. Strings are null-terminated. */
typedef struct {
    const char* key;
    const char* val;
} flux_meta_kv_t;

/* Directed graph edge: target record + relationship. rel is null-terminated. */
typedef struct {
    flux_id_t target;
    const char* rel;
} flux_link_t;

/* The universal atom. Every facet is one of these.
 *
 * Ownership:
 *   - When the CALLER builds a record to put(): all pointers (path, ptype,
 *     kind, meta[].key/val, links[].rel, payload.data) are BORROWED for the
 *     duration of the call only; flux_put copies what it needs.
 *   - When the ENGINE returns a record (flux_get / flux_iter_next): all
 *     pointers point into HEAP MEMORY owned by that record handle; free with
 *     flux_record_free(). Pointers are invalidated once the record is freed.
 */
typedef struct {
    flux_id_t id;          /* filled by engine on put/get */
    flux_layer_t layer;
    flux_pclass_t pclass;
    flux_clock_t clock;
    const char* path;      /* human-readable in-layer path, may be NULL */
    const char* ptype;     /* mime hint, may be NULL */
    const char* kind;      /* canonical or open kind token, may be NULL */

    const flux_meta_kv_t* meta;
    uint32_t meta_count;

    const flux_link_t* links;
    uint32_t link_count;

    flux_buf_t payload;    /* TEXT or BIN */

    uint64_t ts;           /* timestamp */
    uint32_t ver;          /* MVCC version (commit_seq) */
} flux_record_t;

/* Hex codec for ids (32 hex chars + NUL). buf must be >= 33 bytes. */
void flux_id_to_hex(const flux_id_t* id, char* buf33);
flux_status_t flux_id_from_hex(const char* hex, flux_id_t* out);

#ifdef __cplusplus
}
#endif
#endif /* FLUXMEME_TYPES_H */
