/* test_compact — flux_compact reclaims growth: live records survive, tombstones
 * and superseded versions are dropped, the file shrinks. */
#include "fluxmeme/fluxmeme.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg)                                                       \
    do { if (!(cond)) { printf("FAIL: %s (%s)\n", msg, flux_last_error()); goto fail; } } while (0)

static long file_size(const char* p) {
    FILE* f = fopen(p, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    return sz;
}

int main(void) {
    const char* path = "test_compact.flux";
    remove(path);

    flux_store_t* s = NULL;
    CHECK(flux_open(path, 1, &s) == FLUX_OK, "open");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(s, &w) == FLUX_OK, "begin write");

    flux_record_t a = {0}, b = {0}, c = {0};
    a.layer = FLUX_LAYER_MIND; a.kind = "x"; a.payload.data = (const uint8_t*)"aaaa"; a.payload.len = 4;
    b.layer = FLUX_LAYER_MIND; b.kind = "x"; b.payload.data = (const uint8_t*)"bbbb"; b.payload.len = 4;
    c.layer = FLUX_LAYER_MIND; c.kind = "x"; c.payload.data = (const uint8_t*)"cccc"; c.payload.len = 4;
    CHECK(flux_put(w, &a) == FLUX_OK, "put a");
    flux_id_t id_a = a.id;
    CHECK(flux_put(w, &b) == FLUX_OK, "put b");
    flux_id_t id_b = b.id;
    CHECK(flux_put(w, &c) == FLUX_OK, "put c");
    CHECK(flux_del(w, &id_b) == FLUX_OK, "del b");
    /* update a -> creates a superseded older version */
    CHECK(flux_put(w, &a) == FLUX_OK, "update a");
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit");
    flux_close(s);

    long before = file_size(path);
    CHECK(before > 0, "size before");

    CHECK(flux_compact(path) == FLUX_OK, "compact");
    long after = file_size(path);
    CHECK(after > 0 && after < before, "compacted file shrank");

    /* reopen: 2 live records (a + c), b gone */
    CHECK(flux_open(path, 0, &s) == FLUX_OK, "reopen");
    flux_txn_t* r = NULL;
    CHECK(flux_txn_begin_read(s, &r) == FLUX_OK, "begin read");
    flux_iter_t* it = NULL;
    CHECK(flux_scan(r, NULL, &it) == FLUX_OK, "scan");
    int n = 0, b_present = 0;
    flux_record_t rec;
    while (flux_iter_next(it, &rec) == FLUX_OK) {
        n++;
        if (memcmp(rec.id.bytes, id_b.bytes, 16) == 0) b_present = 1;
        flux_record_free(&rec);
    }
    flux_iter_free(it);
    CHECK(n == 2, "2 live records after compact");
    CHECK(!b_present, "deleted b stays gone");
    /* a still resolvable */
    flux_record_t ga = {0};
    CHECK(flux_get(r, &id_a, &ga) == FLUX_OK, "a still present");
    flux_record_free(&ga);

    flux_close(s);
    remove(path);
    printf("PASS\n");
    return 0;
fail:
    if (s) flux_close(s);
    remove(path);
    return 1;
}
