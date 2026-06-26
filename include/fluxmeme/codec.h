/* FLUXmeme — transcoder (codec) registry.
 *
 * Transcoders project the composed record view to/from foreign formats
 * (USD/OKF/A2A/MCAP/MAVLink/URDF...). They are application-layer; adding a
 * new one = adding a codec module, never touching the engine. */
#ifndef FLUXMEME_CODEC_H
#define FLUXMEME_CODEC_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct flux_store flux_store_t;
typedef struct flux_txn flux_txn_t;

/* A transcoder encodes the txn's composed view to an output path/dir, or
 * decodes input into the txn. */
typedef flux_status_t (*flux_encode_fn)(const flux_txn_t* txn, const char* out);
typedef flux_status_t (*flux_decode_fn)(const char* in, flux_txn_t* txn);

typedef struct {
    const char* name; /* "usd", "okf", "a2a", "mcap", "mavlink", ... */
    flux_encode_fn encode;
    flux_decode_fn decode;
} flux_codec_t;

/* Register a transcoder (v1 has 4 built-in; extensible). */
flux_status_t flux_codec_register(const flux_codec_t* codec);
/* Lookup by name (returns NULL if unknown). */
const flux_codec_t* flux_codec_find(const char* name);

#ifdef __cplusplus
}
#endif
#endif /* FLUXMEME_CODEC_H */
