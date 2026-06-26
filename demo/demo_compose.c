/* demo_compose — minimal LIVRPS composition (sublayers + record-level override).
 * base.flux has a link (mass=10); override.flux overrides the SAME id (mass=20);
 * root.flux declares sublayers="override.flux;base.flux" (override stronger).
 * compose_open resolves the merged view (mass=20) while base.flux is untouched. */
#include "fluxmeme/fluxmeme.h"
#include "../src/compose/compose.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHECK(cond, msg)                                                       \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL: %s (%s)\n", msg, flux_last_error());                \
            return 1;                                                         \
        }                                                                     \
    } while (0)

static const uint8_t FIXED_ID[16] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 };

static void put_link(const char* path, const char* mass) {
    flux_store_t* s = NULL;
    flux_open(path, 1, &s);
    flux_txn_t* w = NULL;
    flux_txn_begin_write(s, &w);
    flux_meta_kv_t m[2];
    m[0].key = "name"; m[0].val = "base";
    m[1].key = "mass"; m[1].val = mass;
    flux_record_t r;
    memset(&r, 0, sizeof(r));
    memcpy(r.id.bytes, FIXED_ID, 16);
    r.layer = FLUX_LAYER_BODY;
    r.kind = "robot/link";
    r.meta = m;
    r.meta_count = 2;
    flux_put(w, &r);
    flux_txn_commit(w);
    flux_close(s);
}

static const char* get_mass(flux_store_t* s) {
    flux_txn_t* t = NULL;
    flux_txn_begin_read(s, &t);
    flux_id_t id;
    memcpy(id.bytes, FIXED_ID, 16);
    flux_record_t r;
    static char out[16];
    out[0] = '\0';
    if (flux_get(t, &id, &r) == FLUX_OK) {
        for (uint32_t i = 0; i < r.meta_count; ++i)
            if (r.meta[i].key && strcmp(r.meta[i].key, "mass") == 0)
                strncpy(out, r.meta[i].val, sizeof(out) - 1);
        flux_record_free(&r);
    }
    flux_txn_rollback(t);
    return out;
}

int main(void) {
    put_link("comp_base.flux", "10");
    put_link("comp_override.flux", "20");

    /* root with the compose manifest */
    flux_store_t* root = NULL;
    CHECK(flux_open("comp_root.flux", 1, &root) == FLUX_OK, "open root");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(root, &w) == FLUX_OK, "begin write root");
    flux_meta_kv_t m;
    m.key = "sublayers";
    m.val = "comp_override.flux;comp_base.flux"; /* override first = stronger */
    flux_record_t mr;
    memset(&mr, 0, sizeof(mr));
    mr.layer = FLUX_LAYER_MIND;
    mr.kind = "flux/compose";
    mr.meta = &m;
    mr.meta_count = 1;
    CHECK(flux_put(w, &mr) == FLUX_OK, "put manifest");
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit root");
    flux_close(root); /* release the writable handle before reopening read-only */

    /* open the composed view */
    flux_compose_t* c = NULL;
    CHECK(flux_compose_open("comp_root.flux", &c) == FLUX_OK, "compose_open");
    CHECK(flux_compose_n_layers(c) >= 2, "at least root + 1 sublayer");

    /* resolve: override (mass=20) must win */
    flux_id_t id;
    memcpy(id.bytes, FIXED_ID, 16);
    flux_record_t got;
    CHECK(flux_compose_get(c, &id, &got) == FLUX_OK, "compose_get");
    const char* mv = "";
    for (uint32_t i = 0; i < got.meta_count; ++i)
        if (got.meta[i].key && strcmp(got.meta[i].key, "mass") == 0) mv = got.meta[i].val;
    CHECK(strcmp(mv, "20") == 0, "override (mass=20) wins in composed view");
    flux_record_free(&got);

    /* merged scan: the id appears ONCE (resolved), mass=20 */
    flux_compose_iter_t* it = NULL;
    CHECK(flux_compose_scan(c, NULL, &it) == FLUX_OK, "compose_scan");
    int n_link = 0;
    while (flux_compose_iter_next(it, &got) == FLUX_OK) {
        if (got.kind && strcmp(got.kind, "robot/link") == 0) n_link++;
        flux_record_free(&got);
    }
    flux_compose_iter_free(it);
    CHECK(n_link == 1, "merged view has the link ONCE (deduped by id)");

    /* non-destructive: base.flux still has mass=10 */
    flux_store_t* b = NULL;
    CHECK(flux_open("comp_base.flux", 0, &b) == FLUX_OK, "reopen base");
    CHECK(strcmp(get_mass(b), "10") == 0, "base.flux untouched (mass=10)");
    flux_close(b);

    flux_compose_close(c);
    printf("PASS: demo_compose override wins (mass=20), base untouched, deduped view OK\n");
    return 0;
}
