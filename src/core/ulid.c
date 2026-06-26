/* ULID-like id: 48-bit ms timestamp (big-endian, sort-ordered) + 80-bit random.
 * + hex codec + shared error helper. */
#include "internal.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

/* ---- thread-local-ish error string (single-threaded store handle model) --- */
static char g_errbuf[256] = {0};
void flux_set_error(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_errbuf, sizeof(g_errbuf), fmt, ap);
    va_end(ap);
}
const char* flux_last_error(void) {
    return g_errbuf;
}

/* ---- id generation ---- */
static uint64_t now_ms(void) {
    return (uint64_t)time(NULL) * 1000ULL;
}

void flux_id_gen(flux_id_t* out) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = 1;
    }
    uint64_t ms = now_ms();
    /* big-endian 48-bit ms -> bytes[0..5] for lexical sort order */
    out->bytes[0] = (uint8_t)((ms >> 40) & 0xff);
    out->bytes[1] = (uint8_t)((ms >> 32) & 0xff);
    out->bytes[2] = (uint8_t)((ms >> 24) & 0xff);
    out->bytes[3] = (uint8_t)((ms >> 16) & 0xff);
    out->bytes[4] = (uint8_t)((ms >> 8) & 0xff);
    out->bytes[5] = (uint8_t)(ms & 0xff);
    for (int i = 6; i < 16; ++i)
        out->bytes[i] = (uint8_t)(rand() & 0xff);
}

/* ---- hex codec ---- */
static const char hexd[] = "0123456789abcdef";
void flux_id_to_hex(const flux_id_t* id, char* buf33) {
    for (int i = 0; i < 16; ++i) {
        buf33[i * 2] = hexd[(id->bytes[i] >> 4) & 0xf];
        buf33[i * 2 + 1] = hexd[id->bytes[i] & 0xf];
    }
    buf33[32] = '\0';
}
flux_status_t flux_id_from_hex(const char* hex, flux_id_t* out) {
    if (!hex || !out)
        return FLUX_ERR_ARG;
    for (int i = 0; i < 16; ++i) {
        int hi = -1, lo = -1;
        char a = hex[i * 2], b = hex[i * 2 + 1];
        if (a >= '0' && a <= '9') hi = a - '0';
        else if (a >= 'a' && a <= 'f') hi = a - 'a' + 10;
        else if (a >= 'A' && a <= 'F') hi = a - 'A' + 10;
        if (b >= '0' && b <= '9') lo = b - '0';
        else if (b >= 'a' && b <= 'f') lo = b - 'a' + 10;
        else if (b >= 'A' && b <= 'F') lo = b - 'A' + 10;
        if (hi < 0 || lo < 0)
            return FLUX_ERR_ARG;
        out->bytes[i] = (uint8_t)((hi << 4) | lo);
    }
    return FLUX_OK;
}
