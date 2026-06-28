/* demo_mavlink — JOURNAL/signal <-> MAVLink v2 frames, large-asset filter.
 * Writes 2 signal params + a big mesh, transcodes to MAVLink frames, re-ingests
 * into a fresh store, asserts ONLY the signals crossed the bus (mesh filtered). */
#include "fluxmeme/fluxmeme.h"
#include "../src/core/ref_utils.h"
#include "../src/transcode/usd/stl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL: %s (%s)\n", msg, flux_last_error());                \
            return 1;                                                         \
        }                                                                     \
    } while (0)

static void add_signal(flux_txn_t* w, const char* name, const char* value) {
    flux_meta_kv_t m[2];
    m[0].key = "name";  m[0].val = name;  m[0].type = FLUX_META_STRING;
    m[1].key = "value"; m[1].val = value; m[1].type = FLUX_META_STRING;
    flux_record_t r;
    memset(&r, 0, sizeof(r));
    r.layer = FLUX_LAYER_JOURNAL;
    r.kind = "signal";
    r.meta = m;
    r.meta_count = 2;
    flux_put(w, &r);
}

int main(void) {
    /* a big mesh asset that must NOT cross the bus */
    flux_stl_tri cube[1];
    memset(cube, 0, sizeof(cube));
    cube[0].v[0][0] = 0; cube[0].v[1][0] = 1; cube[0].v[2][0] = 1;
    cube[0].v[0][1] = 0; cube[0].v[1][1] = 1; cube[0].v[2][1] = 1;
    cube[0].n[2] = 1;
    uint8_t* stl = NULL; size_t stl_len = 0;
    flux_stl_encode(cube, 1, &stl, &stl_len);

    flux_store_t* s = NULL;
    CHECK(flux_open("demo_mav.flux", 1, &s) == FLUX_OK, "open");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(s, &w) == FLUX_OK, "begin write");
    add_signal(w, "battery", "12.4");
    add_signal(w, "temp", "42");
    flux_record_t mesh;
    memset(&mesh, 0, sizeof(mesh));
    mesh.layer = FLUX_LAYER_BODY;
    mesh.pclass = FLUX_PCLASS_BIN;
    mesh.kind = "mesh";
    mesh.ptype = "model/stl";
    mesh.payload.data = stl;
    mesh.payload.len = stl_len;
    CHECK(flux_put(w, &mesh) == FLUX_OK, "put mesh");
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit");

    /* transcode out */
    flux_txn_t* r1 = NULL;
    CHECK(flux_txn_begin_read(s, &r1) == FLUX_OK, "begin read");
    CHECK(flux_to_mavlink(r1, "mav.frames") == FLUX_OK, "to_mavlink");

    FILE* tf = fopen("mav.frames", "rb");
    CHECK(tf != NULL, "frames written");
    fseek(tf, 0, SEEK_END);
    CHECK(ftell(tf) > 0, "frames non-empty");
    fclose(tf);

    /* re-ingest into a fresh store */
    flux_store_t* s2 = NULL;
    CHECK(flux_open("demo_mav2.flux", 1, &s2) == FLUX_OK, "open store2");
    flux_txn_t* w2 = NULL;
    CHECK(flux_txn_begin_write(s2, &w2) == FLUX_OK, "begin write2");
    CHECK(flux_from_mavlink("mav.frames", w2) == FLUX_OK, "from_mavlink");
    CHECK(flux_txn_commit(w2) == FLUX_OK, "commit2");

    /* verify: 2 signals back, NO mesh */
    flux_txn_t* r2 = NULL;
    CHECK(flux_txn_begin_read(s2, &r2) == FLUX_OK, "begin read2");
    flux_iter_t* it = NULL;
    CHECK(flux_scan(r2, NULL, &it) == FLUX_OK, "scan");
    int n_signal = 0, n_mesh = 0, vals_ok = 0;
    flux_record_t rec;
    while (flux_iter_next(it, &rec) == FLUX_OK) {
        if (rec.kind && strcmp(rec.kind, "signal") == 0) {
            n_signal++;
            const char* v = "";
            for (uint32_t i = 0; i < rec.meta_count; ++i)
                if (rec.meta[i].key && strcmp(rec.meta[i].key, "value") == 0) v = rec.meta[i].val;
            if (strcmp(v, "12.4") == 0 || strcmp(v, "42") == 0) vals_ok++;
        } else if (rec.kind && strcmp(rec.kind, "mesh") == 0) {
            n_mesh++;
        }
        flux_record_free(&rec);
    }
    flux_iter_free(it);

    CHECK(n_signal == 2, "2 signals crossed the bus");
    CHECK(vals_ok == 2, "signal values preserved");
    CHECK(n_mesh == 0, "mesh correctly filtered out (not on the bus)");

    flux_close(s2);
    flux_close(s);
    free(stl);
    printf("PASS: demo_mavlink 2 signals over bus, mesh filtered (MAVLink v2 framing) OK\n");
    return 0;
}
