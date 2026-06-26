/* Binary STL codec. Format: 80-byte header, u32 triangle count, then
 * 50 bytes/triangle (12 normal + 36 verts + 2 attribute). Little-endian. */
#include "stl.h"
#include <stdlib.h>
#include <string.h>

static float rd_f32(const uint8_t* p) {
    /* little-endian float */
    uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
                 ((uint32_t)p[3] << 24);
    float f;
    memcpy(&f, &u, 4);
    return f;
}
static void wr_f32(uint8_t* p, float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    p[0] = (uint8_t)(u & 0xff);
    p[1] = (uint8_t)((u >> 8) & 0xff);
    p[2] = (uint8_t)((u >> 16) & 0xff);
    p[3] = (uint8_t)((u >> 24) & 0xff);
}

int flux_stl_decode(const uint8_t* buf, size_t len, flux_stl_tri** tris, uint32_t* count) {
    if (!buf || !tris || !count) return -1;
    if (len < 84) return -1;
    uint32_t n = (uint32_t)buf[80] | ((uint32_t)buf[81] << 8) | ((uint32_t)buf[82] << 16) |
                 ((uint32_t)buf[83] << 24);
    if (len < 84 + (size_t)n * 50) return -1;
    flux_stl_tri* t = (flux_stl_tri*)malloc((size_t)(n ? n : 1) * sizeof(flux_stl_tri));
    if (!t) return -1;
    const uint8_t* p = buf + 84;
    for (uint32_t i = 0; i < n; ++i) {
        t[i].n[0] = rd_f32(p); t[i].n[1] = rd_f32(p + 4); t[i].n[2] = rd_f32(p + 8);
        for (int j = 0; j < 3; ++j) {
            const uint8_t* q = p + 12 + j * 12;
            t[i].v[j][0] = rd_f32(q);
            t[i].v[j][1] = rd_f32(q + 4);
            t[i].v[j][2] = rd_f32(q + 8);
        }
        p += 50;
    }
    *tris = t;
    *count = n;
    return 0;
}

int flux_stl_encode(const flux_stl_tri* tris, uint32_t count, uint8_t** out, size_t* out_len) {
    if (!tris || !out || !out_len) return -1;
    size_t sz = 84 + (size_t)count * 50;
    uint8_t* buf = (uint8_t*)malloc(sz);
    if (!buf) return -1;
    memset(buf, 0, 80); /* header */
    memcpy(buf, "FLUXmeme binary STL", 19);
    buf[80] = (uint8_t)(count & 0xff);
    buf[81] = (uint8_t)((count >> 8) & 0xff);
    buf[82] = (uint8_t)((count >> 16) & 0xff);
    buf[83] = (uint8_t)((count >> 24) & 0xff);
    uint8_t* p = buf + 84;
    for (uint32_t i = 0; i < count; ++i) {
        wr_f32(p, tris[i].n[0]); wr_f32(p + 4, tris[i].n[1]); wr_f32(p + 8, tris[i].n[2]);
        for (int j = 0; j < 3; ++j) {
            uint8_t* q = p + 12 + j * 12;
            wr_f32(q, tris[i].v[j][0]);
            wr_f32(q + 4, tris[i].v[j][1]);
            wr_f32(q + 8, tris[i].v[j][2]);
        }
        p[48] = 0; p[49] = 0; /* attribute */
        p += 50;
    }
    *out = buf;
    *out_len = sz;
    return 0;
}
