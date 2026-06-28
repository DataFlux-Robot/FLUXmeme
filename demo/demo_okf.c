/* demo_okf — store -> OKF markdown bundle -> store round-trip.
 * Builds 3 MIND/concept records (A links to B), transcodes out to okf_out/,
 * re-ingests into a fresh store, and asserts the round-trip. */
#include "fluxmeme/fluxmeme.h"
#include "../src/core/ref_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL: %s (%s)\n", msg, flux_last_error());                 \
            return 1;                                                          \
        }                                                                      \
    } while (0)

/* title/tags/body are string literals (static storage -> stable pointers). */
static void make_concept(flux_record_t* r, flux_meta_kv_t meta[3],
                         const char* title, const char* body, const char* tags) {
    memset(r, 0, sizeof(*r));
    r->layer = FLUX_LAYER_MIND;
    r->pclass = FLUX_PCLASS_TEXT;
    r->kind = "concept";
    r->ptype = "text/markdown";
    meta[0].key = "type";  meta[0].val = "concept";   meta[0].type = FLUX_META_STRING;
    meta[1].key = "title"; meta[1].val = title;       meta[1].type = FLUX_META_STRING;
    meta[2].key = "tags";  meta[2].val = tags;        meta[2].type = FLUX_META_STRING;
    r->meta = meta;
    r->meta_count = 3;
    r->payload.data = (const uint8_t*)body;
    r->payload.len = strlen(body);
}

int main(void) {
    flux_store_t* s = NULL;
    CHECK(flux_open("demo_okf.flux", 1, &s) == FLUX_OK, "open store");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(s, &w) == FLUX_OK, "begin write");

    flux_meta_kv_t ma[4], mb[3], mc[3];
    flux_record_t a, b, c;
    make_concept(&a, ma, "A", "# A", "x,y");
    make_concept(&b, mb, "B", "# B", "x");
    make_concept(&c, mc, "C", "# C", "y");
    CHECK(flux_put(w, &a) == FLUX_OK, "put A");
    CHECK(flux_put(w, &b) == FLUX_OK, "put B");
    /* link A -> B (references) as a REF-typed meta entry */
    char a_hex[33]; char a_ref[80];
    flux_id_to_hex(&b.id, a_hex);
    flux_ref_encode(a_ref, sizeof(a_ref), a_hex, NULL);
    ma[3].key = "references"; ma[3].val = a_ref; ma[3].type = FLUX_META_REF;
    a.meta = ma; a.meta_count = 4;
    CHECK(flux_put(w, &a) == FLUX_OK, "put A (with link)");
    CHECK(flux_put(w, &c) == FLUX_OK, "put C");
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit");

    /* transcode out */
    flux_txn_t* r1 = NULL;
    CHECK(flux_txn_begin_read(s, &r1) == FLUX_OK, "begin read");
    CHECK(flux_to_okf(r1, "okf_out") == FLUX_OK, "to_okf");

    /* ingest into a fresh store */
    flux_store_t* s2 = NULL;
    CHECK(flux_open("demo_okf2.flux", 1, &s2) == FLUX_OK, "open store2");
    flux_txn_t* w2 = NULL;
    CHECK(flux_txn_begin_write(s2, &w2) == FLUX_OK, "begin write2");
    CHECK(flux_from_okf("okf_out", w2) == FLUX_OK, "from_okf");
    CHECK(flux_txn_commit(w2) == FLUX_OK, "commit2");

    /* verify round-trip */
    flux_txn_t* r2 = NULL;
    CHECK(flux_txn_begin_read(s2, &r2) == FLUX_OK, "begin read2");
    flux_filter_t f;
    memset(&f, 0, sizeof(f));
    f.layer_mask = FLUX_LAYER_MIND;
    f.kind = "concept";
    flux_iter_t* it = NULL;
    CHECK(flux_scan(r2, &f, &it) == FLUX_OK, "scan");

    int count = 0, titles_ok = 0, payloads_ok = 0, link_ok = 0;
    flux_record_t rec;
    while (flux_iter_next(it, &rec) == FLUX_OK) {
        count++;
        for (uint32_t i = 0; i < rec.meta_count; ++i) {
            if (strcmp(rec.meta[i].key, "title") == 0) {
                titles_ok += (rec.meta[i].val[0] >= 'A' && rec.meta[i].val[0] <= 'C');
            }
        }
        /* body preserved: header "# X" (letter at index 2); record A also
         * carries a ## Links section per OKF wikilinks-in-body convention. */
        if (rec.payload.len >= 3 && rec.payload.data[0] == '#' &&
            rec.payload.data[1] == ' ' &&
            rec.payload.data[2] >= 'A' && rec.payload.data[2] <= 'C')
            payloads_ok++;
        if (flux_ref_count(&rec) >= 1) link_ok++;
        flux_record_free(&rec);
    }
    flux_iter_free(it);

    CHECK(count == 3, "expected 3 concepts back");
    CHECK(titles_ok == 3, "titles recovered");
    CHECK(payloads_ok == 3, "payloads recovered (#A/#B/#C)");
    CHECK(link_ok == 1, "A->B link round-tripped");

    flux_close(s2);
    flux_close(s);
    printf("PASS: demo_okf round-trip (3 concepts, titles+payloads+link) OK\n");
    return 0;
}
