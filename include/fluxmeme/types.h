/* FLUXmeme v2 — core types. See SPEC.md.
 *
 * v2 change: links[] DELETED. Connections are REF-typed meta entries.
 * One mechanism (typed meta KV) carries everything: properties, connections,
 * structured data. Multi-topology via @graph suffix on REF values.
 */
#ifndef FLUXMEME_TYPES_H
#define FLUXMEME_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Layers (bitset; the three "natures" of an embodied node) --- */
typedef enum {
    FLUX_LAYER_BODY    = 0x01,
    FLUX_LAYER_MIND    = 0x02,
    FLUX_LAYER_JOURNAL = 0x04
} flux_layer_t;

/* --- Payload class --- */
typedef enum {
    FLUX_PCLASS_TEXT = 1,
    FLUX_PCLASS_BIN  = 2
} flux_pclass_t;

/* --- Clock domain --- */
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
    FLUX_ERR_VERSION,
    FLUX_ERR_NOMEM,
    FLUX_ERR_RANGE,
    FLUX_ERR_CORRUPT
} flux_status_t;

/* --- Meta value types (v2) --- */
typedef enum {
    FLUX_META_STRING = 0,  /* "revolute" */
    FLUX_META_INT    = 1,  /* "42" → int64 */
    FLUX_META_FLOAT  = 2,  /* "23.7" → double */
    FLUX_META_BOOL   = 3,  /* "true" / "false" */
    FLUX_META_JSON   = 4,  /* "{\"pin1\":\"CAN_H\"}" — embedded structured data */
    FLUX_META_REF    = 5,  /* "019f0411...3ec@mechanical" — cross-record link with @graph */
} flux_meta_type_t;

/* 16-byte ULID-like identity. */
typedef struct {
    uint8_t bytes[16];
} flux_id_t;

/* Non-owning byte view. */
typedef struct {
    const uint8_t* data;
    size_t len;
} flux_buf_t;

/* v2 meta KV: key + string-encoded val + type tag.
 * For REF type, val = "32-hex-id@graph" (graph optional; default = "").
 * Multiple entries with same key + type=REF = multi-valued connection (e.g. children).
 */
typedef struct {
    const char*      key;
    const char*      val;
    flux_meta_type_t type;
} flux_meta_kv_t;

/* The universal atom (v2). NO links[] — connections are REF-typed meta.
 *
 * Ownership:
 *   - CALLER builds: pointers borrowed for the call; flux_put copies.
 *   - ENGINE returns: heap copy; free with flux_record_free().
 */
typedef struct {
    flux_id_t id;
    flux_layer_t layer;
    flux_pclass_t pclass;
    flux_clock_t clock;
    const char* path;      /* may be NULL */
    const char* ptype;     /* mime hint, may be NULL */
    const char* kind;      /* canonical or open kind, may be NULL */

    const flux_meta_kv_t* meta;   /* ALL data: properties + connections + JSON */
    uint32_t meta_count;
    /* links[] DELETED in v2 — use REF-typed meta */

    flux_buf_t payload;

    uint64_t ts;
    uint32_t ver;
} flux_record_t;

/* Hex codec for ids (32 hex chars + NUL). buf must be >= 33 bytes. */
void flux_id_to_hex(const flux_id_t* id, char* buf33);
flux_status_t flux_id_from_hex(const char* hex, flux_id_t* out);

#ifdef __cplusplus
}
#endif
#endif /* FLUXMEME_TYPES_H */
