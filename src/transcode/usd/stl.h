/* Binary STL codec (little-endian). Used by the USD transcoder to map
 * BODY/kind=mesh BIN payloads (model/stl) to/from USD Mesh prims. */
#ifndef FLUX_STL_H
#define FLUX_STL_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    float n[3];       /* facet normal */
    float v[3][3];    /* 3 vertices, each xyz */
} flux_stl_tri;

/* decode STL bytes -> triangle array (malloc'd). returns 0 ok, -1 bad. */
int flux_stl_decode(const uint8_t* buf, size_t len, flux_stl_tri** tris, uint32_t* count);
/* encode triangle array -> STL bytes (malloc'd, *out_len set). returns 0 ok, -1 bad. */
int flux_stl_encode(const flux_stl_tri* tris, uint32_t count, uint8_t** out, size_t* out_len);

#endif
