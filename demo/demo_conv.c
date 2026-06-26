/* demo_conv — .flux binary <-> .fluxa text canonical source round-trip.
 * Writes a TEXT concept (with embedded newline + link) and a small BIN record,
 * converts to .fluxa, re-ingests, asserts fields/payload/link survive. */
#include "fluxmeme/fluxmeme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHECK(cond, msg)                                                       \
    do { if (!(cond)) { printf("FAIL: %s (%s)\n", msg, flux_last_error()); return 1; } } while (0)

int main(void) {
    flux_store_t* s = NULL;
    CHECK(flux_open("demo_conv.flux", 1, &s) == FLUX_OK, "open store");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(s, &w) == FLUX_OK, "begin write");

    /* a small BIN record first (we need its id for the concept's link) */
    static const uint8_t bin[4] = { 0x00, 0xff, 0x11, 0x22 };
    flux_record_t m;
    memset(&m, 0, sizeof(m));
    m.layer = FLUX_LAYER_BODY;
    m.pclass = FLUX_PCLASS_BIN;
    m.kind = "mesh";
    m.ptype = "model/stl";
    m.payload.data = bin;
    m.payload.len = 4;
    CHECK(flux_put(w, &m) == FLUX_OK, "put bin");
    flux_id_t mesh_id = m.id;

    /* concept TEXT with embedded newline + a link to the mesh */
    flux_meta_kv_t meta[1] = { {"title", "Hello"} };
    flux_link_t link;
    memset(&link, 0, sizeof(link));
    memcpy(link.target.bytes, mesh_id.bytes, 16);
    link.rel = "references";
    flux_record_t c;
    memset(&c, 0, sizeof(c));
    c.layer = FLUX_LAYER_MIND;
    c.pclass = FLUX_PCLASS_TEXT;
    c.kind = "concept";
    c.meta = meta;
    c.meta_count = 1;
    c.links = &link;
    c.link_count = 1;
    c.payload.data = (const uint8_t*)"hello\nworld";
    c.payload.len = 11;
    CHECK(flux_put(w, &c) == FLUX_OK, "put concept");
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit");

    /* binary -> .fluxa */
    flux_txn_t* r1 = NULL;
    CHECK(flux_txn_begin_read(s, &r1) == FLUX_OK, "begin read");
    CHECK(flux_conv_to_fluxa(r1, "demo.fluxa") == FLUX_OK, "to_fluxa");

    /* .fluxa -> fresh binary store */
    flux_store_t* s2 = NULL;
    CHECK(flux_open("demo_conv2.flux", 1, &s2) == FLUX_OK, "open store2");
    flux_txn_t* w2 = NULL;
    CHECK(flux_txn_begin_write(s2, &w2) == FLUX_OK, "begin write2");
    CHECK(flux_conv_from_fluxa("demo.fluxa", w2) == FLUX_OK, "from_fluxa");
    CHECK(flux_txn_commit(w2) == FLUX_OK, "commit2");

    /* verify */
    flux_txn_t* r2 = NULL;
    CHECK(flux_txn_begin_read(s2, &r2) == FLUX_OK, "begin read2");
    flux_iter_t* it = NULL;
    CHECK(flux_scan(r2, NULL, &it) == FLUX_OK, "scan");
    int n = 0, text_ok = 0, bin_ok = 0, link_ok = 0;
    flux_record_t rec;
    while (flux_iter_next(it, &rec) == FLUX_OK) {
        n++;
        if (rec.pclass == FLUX_PCLASS_TEXT && rec.kind && strcmp(rec.kind, "concept") == 0) {
            if (rec.payload.len == 11 && memcmp(rec.payload.data, "hello\nworld", 11) == 0)
                text_ok++;
            if (rec.link_count >= 1) link_ok++;
        }
        if (rec.pclass == FLUX_PCLASS_BIN && rec.kind && strcmp(rec.kind, "mesh") == 0) {
            if (rec.payload.len == 4 && memcmp(rec.payload.data, bin, 4) == 0)
                bin_ok++;
        }
        flux_record_free(&rec);
    }
    flux_iter_free(it);

    CHECK(n == 2, "2 records after round-trip");
    CHECK(text_ok == 1, "TEXT concept payload (with newline) preserved");
    CHECK(bin_ok == 1, "BIN payload (hex) preserved");
    CHECK(link_ok == 1, "concept->mesh link preserved");

    flux_close(s2);
    flux_close(s);
    printf("PASS: demo_conv .flux <-> .fluxa round-trip (TEXT+BIN payload + link) OK\n");
    return 0;
}
