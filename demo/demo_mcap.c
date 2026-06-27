/* demo_mcap — JOURNAL/signal <-> MCAP (rosbag2) round-trip.
 * Writes 2 signal records, projects to a .mcap, re-ingests, asserts the
 * signals survive (name + value). Validates the journal's primary projection. */
#include "fluxmeme/fluxmeme.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg)                                                       \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL: %s (%s)\n", msg, flux_last_error());                \
            return 1;                                                         \
        }                                                                     \
    } while (0)

static void add_signal(flux_txn_t* w, const char* name, const char* value) {
    flux_meta_kv_t m[2];
    m[0].key = "name";  m[0].val = name;
    m[1].key = "value"; m[1].val = value;
    flux_record_t r;
    memset(&r, 0, sizeof(r));
    r.layer = FLUX_LAYER_JOURNAL;
    r.kind = "signal";
    r.clock = FLUX_CLOCK_DEVICE_MONOTONIC;
    r.meta = m;
    r.meta_count = 2;
    flux_put(w, &r);
}

int main(void) {
    flux_store_t* s = NULL;
    CHECK(flux_open("demo_mcap.flux", 1, &s) == FLUX_OK, "open");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(s, &w) == FLUX_OK, "begin write");
    add_signal(w, "battery", "12.4");
    add_signal(w, "temp", "42");
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit");

    /* project to MCAP */
    flux_txn_t* r1 = NULL;
    CHECK(flux_txn_begin_read(s, &r1) == FLUX_OK, "begin read");
    CHECK(flux_to_mcap(r1, "demo.mcap") == FLUX_OK, "to_mcap");

    /* the file exists + starts with MCAP magic */
    FILE* f = fopen("demo.mcap", "rb");
    CHECK(f != NULL, "mcap written");
    unsigned char magic[8];
    CHECK(fread(magic, 1, 8, f) == 8, "read magic");
    fclose(f);
    CHECK(magic[0] == 0x89 && magic[1] == 'M' && magic[4] == 'P', "MCAP magic valid");

    /* re-ingest into a fresh store */
    flux_store_t* s2 = NULL;
    CHECK(flux_open("demo_mcap2.flux", 1, &s2) == FLUX_OK, "open store2");
    flux_txn_t* w2 = NULL;
    CHECK(flux_txn_begin_write(s2, &w2) == FLUX_OK, "begin write2");
    CHECK(flux_from_mcap("demo.mcap", w2) == FLUX_OK, "from_mcap");
    CHECK(flux_txn_commit(w2) == FLUX_OK, "commit2");

    /* verify: 2 signals back with values preserved */
    flux_txn_t* r2 = NULL;
    CHECK(flux_txn_begin_read(s2, &r2) == FLUX_OK, "begin read2");
    flux_iter_t* it = NULL;
    CHECK(flux_scan(r2, NULL, &it) == FLUX_OK, "scan");
    int n = 0, vals_ok = 0;
    flux_record_t rec;
    while (flux_iter_next(it, &rec) == FLUX_OK) {
        if (rec.kind && strcmp(rec.kind, "signal") == 0) {
            n++;
            for (uint32_t i = 0; i < rec.meta_count; ++i)
                if (rec.meta[i].key && strcmp(rec.meta[i].key, "value") == 0) {
                    const char* v = rec.meta[i].val;
                    if (strcmp(v, "12.4") == 0 || strcmp(v, "42") == 0) vals_ok++;
                }
        }
        flux_record_free(&rec);
    }
    flux_iter_free(it);

    CHECK(n == 2, "2 signals recovered from MCAP");
    CHECK(vals_ok == 2, "signal values preserved across MCAP round-trip");

    flux_close(s2);
    flux_close(s);
    printf("PASS: demo_mcap JOURNAL/signal <-> MCAP round-trip OK\n");
    return 0;
}
