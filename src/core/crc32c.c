/* CRC32C (Castagnoli) — software table implementation. */
#include "internal.h"

static uint32_t crc32c_table[256];
static int crc32c_table_ready = 0;

static void crc32c_init(void) {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0x82f63b78u ^ (c >> 1)) : (c >> 1);
        crc32c_table[i] = c;
    }
    crc32c_table_ready = 1;
}

uint32_t flux_crc32c_continue(uint32_t seed, const void* data, size_t len) {
    if (!crc32c_table_ready)
        crc32c_init();
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = seed ^ 0xffffffffu;
    for (size_t i = 0; i < len; ++i)
        crc = crc32c_table[(crc ^ p[i]) & 0xff] ^ (crc >> 8);
    return crc ^ 0xffffffffu;
}

uint32_t flux_crc32c(const void* data, size_t len) {
    return flux_crc32c_continue(0, data, len);
}
