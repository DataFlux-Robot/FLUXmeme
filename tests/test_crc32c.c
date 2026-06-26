/* test_crc32c — known-answer test (CRC32C of "123456789" = 0xe3069283). */
#include "../src/core/internal.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* s = "123456789";
    uint32_t c = flux_crc32c(s, strlen(s));
    printf("crc32c(\"123456789\") = 0x%08x (expect 0xe3069283)\n", c);
    if (c != 0xe3069283u) {
        printf("FAIL\n");
        return 1;
    }
    /* continue API */
    uint32_t c2 = flux_crc32c_continue(flux_crc32c("1234", 4), "56789", 5);
    if (c2 != c) { printf("FAIL: continue mismatch 0x%08x\n", c2); return 1; }
    printf("PASS\n");
    return 0;
}
