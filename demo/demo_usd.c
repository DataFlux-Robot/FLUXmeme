/* demo_usd — BODY/mesh (binary STL cube) -> USDA -> store round-trip.
 * Writes a 12-triangle cube as a BODY/mesh BIN payload + a usda_snippet,
 * transcodes out to scene.usda, re-ingests, asserts the cube round-trips. */
#include "fluxmeme/fluxmeme.h"
#include "../src/transcode/usd/stl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHECK(cond, msg)                                                       \
    do { if (!(cond)) { printf("FAIL: %s (%s)\n", msg, flux_last_error()); return 1; } } while (0)

/* a unit cube as 12 triangles: { normal[3], { vert0[3], vert1[3], vert2[3] } } */
static const flux_stl_tri CUBE[12] = {
    /* +Z */ { {0,0,1},  { {0,0,1},{1,0,1},{1,1,1} } },
    { {0,0,1},  { {0,0,1},{1,1,1},{0,1,1} } },
    /* -Z */ { {0,0,-1}, { {0,0,0},{0,1,0},{1,1,0} } },
    { {0,0,-1}, { {0,0,0},{1,1,0},{1,0,0} } },
    /* +X */ { {1,0,0},  { {1,0,0},{1,0,1},{1,1,1} } },
    { {1,0,0},  { {1,0,0},{1,1,1},{1,1,0} } },
    /* -X */ { {-1,0,0}, { {0,0,0},{0,1,0},{0,1,1} } },
    { {-1,0,0}, { {0,0,0},{0,1,1},{0,0,1} } },
    /* +Y */ { {0,1,0},  { {0,1,0},{1,1,0},{1,1,1} } },
    { {0,1,0},  { {0,1,0},{1,1,1},{0,1,1} } },
    /* -Y */ { {0,-1,0}, { {0,0,0},{1,0,0},{1,0,1} } },
    { {0,-1,0}, { {0,0,0},{1,0,1},{0,0,1} } },
};

int main(void) {
    uint8_t* stl = NULL; size_t stl_len = 0;
    CHECK(flux_stl_encode(CUBE, 12, &stl, &stl_len) == 0, "encode cube STL");

    flux_store_t* s = NULL;
    CHECK(flux_open("demo_usd.flux", 1, &s) == FLUX_OK, "open store");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(s, &w) == FLUX_OK, "begin write");

    flux_record_t mesh;
    memset(&mesh, 0, sizeof(mesh));
    mesh.layer = FLUX_LAYER_BODY;
    mesh.pclass = FLUX_PCLASS_BIN;
    mesh.kind = "mesh";
    mesh.ptype = "model/stl";
    mesh.payload.data = stl;
    mesh.payload.len = stl_len;
    CHECK(flux_put(w, &mesh) == FLUX_OK, "put mesh");

    flux_record_t snip;
    memset(&snip, 0, sizeof(snip));
    snip.layer = FLUX_LAYER_BODY;
    snip.pclass = FLUX_PCLASS_TEXT;
    snip.kind = "usda_snippet";
    const char* snippet = "    def Scope \"note\" { string fluxmeme = \"cube asset\" }";
    snip.payload.data = (const uint8_t*)snippet;
    snip.payload.len = strlen(snippet);
    CHECK(flux_put(w, &snip) == FLUX_OK, "put snippet");
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit");

    /* transcode out */
    flux_txn_t* r1 = NULL;
    CHECK(flux_txn_begin_read(s, &r1) == FLUX_OK, "begin read");
    CHECK(flux_to_usd(r1, "scene.usda") == FLUX_OK, "to_usd");

    /* file exists + contains Mesh */
    FILE* tf = fopen("scene.usda", "rb");
    CHECK(tf != NULL, "scene.usda written");
    char head[512]; size_t hn = fread(head, 1, sizeof(head) - 1, tf); head[hn] = '\0';
    fclose(tf);
    CHECK(strstr(head, "def Mesh") != NULL, "USDA has def Mesh");

    /* ingest into a fresh store */
    flux_store_t* s2 = NULL;
    CHECK(flux_open("demo_usd2.flux", 1, &s2) == FLUX_OK, "open store2");
    flux_txn_t* w2 = NULL;
    CHECK(flux_txn_begin_write(s2, &w2) == FLUX_OK, "begin write2");
    CHECK(flux_from_usd("scene.usda", w2) == FLUX_OK, "from_usd");
    CHECK(flux_txn_commit(w2) == FLUX_OK, "commit2");

    /* verify: mesh decodes to 12 triangles */
    flux_txn_t* r2 = NULL;
    CHECK(flux_txn_begin_read(s2, &r2) == FLUX_OK, "begin read2");
    flux_filter_t f;
    memset(&f, 0, sizeof(f));
    f.layer_mask = FLUX_LAYER_BODY;
    f.kind = "mesh";
    flux_iter_t* it = NULL;
    CHECK(flux_scan(r2, &f, &it) == FLUX_OK, "scan mesh");
    flux_record_t rec;
    int found_mesh = 0, tris_ok = 0;
    while (flux_iter_next(it, &rec) == FLUX_OK) {
        found_mesh++;
        flux_stl_tri* tris = NULL; uint32_t n = 0;
        if (flux_stl_decode(rec.payload.data, rec.payload.len, &tris, &n) == 0) {
            if (n == 12) tris_ok = 1;
            free(tris);
        }
        flux_record_free(&rec);
    }
    flux_iter_free(it);

    CHECK(found_mesh >= 1, "mesh re-ingested");
    CHECK(tris_ok, "cube round-trips as 12 triangles");

    flux_close(s2);
    flux_close(s);
    free(stl);
    printf("PASS: demo_usd round-trip (cube 12-tri STL -> USDA -> STL) OK\n");
    return 0;
}
