/* Content hash — 32-byte, deterministic.
 *
 * v1.0 SLICE: a compact, fast, non-cryptographic hash (two-stream FNV-1a + split
 * mix) filling 32 bytes. Sufficient for dedup / integrity / provenance in the
 * reference build. The interface is stable (flux_hash(in, len, out[32])) so a
 * vendored **blake3** can be dropped in later under the same signature without
 * touching callers. (Marked in SPEC.md §2 as blake3-compatible interface.) */
#include "internal.h"

static const uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;
static const uint64_t FNV_PRIME = 0x100000001b3ULL;

static uint64_t splitmix64(uint64_t* state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void flux_hash(const void* data, size_t len, uint8_t out[32]) {
    const uint8_t* p = (const uint8_t*)data;
    /* Four independent streams to fill 4 x 64-bit = 32 bytes. */
    uint64_t s0 = FNV_OFFSET ^ 0xA;
    uint64_t s1 = FNV_OFFSET ^ 0xB;
    uint64_t s2 = FNV_OFFSET ^ 0xC;
    uint64_t s3 = FNV_OFFSET ^ 0xD;
    for (size_t i = 0; i < len; ++i) {
        uint8_t b = p[i];
        s0 = (s0 ^ b) * FNV_PRIME;
        s1 = (s1 + ((uint64_t)b << (i & 7))) * FNV_PRIME ^ (s0 >> 17);
        s2 = (s2 * 0x9e3779b97f4a7c15ULL) ^ b ^ (uint64_t)i;
        s3 = (s3 ^ (b + 0x9e)) * 0xd6e8feb86659fd93ULL;
    }
    /* Final mixing via splitmix to diffuse. */
    uint64_t seed = s0 ^ s1 ^ s2 ^ s3 ^ (uint64_t)len;
    uint64_t h[4];
    h[0] = splitmix64(&seed) ^ s0;
    h[1] = splitmix64(&seed) ^ s1;
    h[2] = splitmix64(&seed) ^ s2;
    h[3] = splitmix64(&seed) ^ s3;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 8; ++j)
            out[i * 8 + j] = (uint8_t)((h[i] >> (8 * j)) & 0xff);
    }
}
