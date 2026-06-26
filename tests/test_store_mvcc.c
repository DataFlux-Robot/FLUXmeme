/* test_store_mvcc — put/commit/get/scan/del/cursor over a real .flux file. */
#include "fluxmeme/fluxmeme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHECK(cond, msg)                                                       \
    do { if (!(cond)) { printf("FAIL: %s (%s)\n", msg, flux_last_error()); goto fail; } } while (0)

int main(void) {
    flux_store_t* s = NULL;
    CHECK(flux_open("test_mvcc.flux", 1, &s) == FLUX_OK, "open");

    flux_id_t id_a = {0}, id_b = {0};
    /* write two records */
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(s, &w) == FLUX_OK, "begin write");
    flux_meta_kv_t ma[1] = { {"title", "A"} };
    flux_record_t a = {0};
    a.layer = FLUX_LAYER_BODY; a.kind = "link"; a.meta = ma; a.meta_count = 1;
    a.payload.data = (const uint8_t*)"base"; a.payload.len = 4;
    CHECK(flux_put(w, &a) == FLUX_OK, "put a");
    id_a = a.id;
    uint32_t ver_a = a.ver;
    flux_record_t b = {0};
    b.layer = FLUX_LAYER_BODY; b.kind = "link"; b.payload.data = (const uint8_t*)"arm"; b.payload.len = 3;
    CHECK(flux_put(w, &b) == FLUX_OK, "put b");
    id_b = b.id;
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit");
    CHECK(flux_commit_seq(s) == 1, "commit_seq == 1");

    /* snapshot read sees version 1 */
    flux_txn_t* r1 = NULL;
    CHECK(flux_txn_begin_read(s, &r1) == FLUX_OK, "begin read1");
    flux_record_t got = {0};
    CHECK(flux_get(r1, &id_a, &got) == FLUX_OK, "get a");
    CHECK(got.ver == 1, "a.ver == 1");
    CHECK(got.payload.len == 4 && memcmp(got.payload.data, "base", 4) == 0, "a payload");
    flux_record_free(&got);

    /* scan returns 2 BODY records */
    flux_filter_t f = {0};
    f.layer_mask = FLUX_LAYER_BODY;
    flux_iter_t* it = NULL;
    CHECK(flux_scan(r1, &f, &it) == FLUX_OK, "scan");
    int n = 0;
    while (flux_iter_next(it, &got) == FLUX_OK) { n++; flux_record_free(&got); }
    flux_iter_free(it);
    CHECK(n == 2, "scan count == 2");

    /* FIFO cursor yields both records from seq 0 */
    flux_cursor_t* cur = NULL;
    CHECK(flux_cursor_open(s, 0, &cur) == FLUX_OK, "cursor open");
    int cn = 0;
    while (flux_cursor_next(cur, &got) == FLUX_OK) { cn++; flux_record_free(&got); }
    flux_cursor_free(cur);
    CHECK(cn == 2, "cursor count == 2");

    /* delete a, then a fresh read no longer finds it */
    flux_txn_t* w2 = NULL;
    CHECK(flux_txn_begin_write(s, &w2) == FLUX_OK, "begin write2");
    CHECK(flux_del(w2, &id_a) == FLUX_OK, "del a");
    CHECK(flux_txn_commit(w2) == FLUX_OK, "commit2");

    flux_txn_t* r2 = NULL;
    CHECK(flux_txn_begin_read(s, &r2) == FLUX_OK, "begin read2");
    flux_status_t gst = flux_get(r2, &id_a, &got);
    CHECK(gst == FLUX_ERR_NOTFOUND, "a deleted");
    /* but b is still there */
    CHECK(flux_get(r2, &id_b, &got) == FLUX_OK, "get b still");
    flux_record_free(&got);

    (void)ver_a;
    /* CAS: concurrent update detected (must run in a write txn) */
    flux_txn_t* w3 = NULL;
    CHECK(flux_txn_begin_write(s, &w3) == FLUX_OK, "begin write3");
    CHECK(flux_cas(w3, &id_b, 999, &b) == FLUX_ERR_VERSION, "cas wrong ver rejected");
    flux_txn_rollback(w3);

    flux_close(s);
    printf("PASS\n");
    return 0;
fail:
    if (s) flux_close(s);
    return 1;
}
