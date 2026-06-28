/* demo_livrps — full LIVRPS: field-level merge + variant selection.
 * base {name, color=red, mass=10}; variant layers override ONLY mass
 *   heavy {flux_variant=heavy, mass=50}, light {flux_variant=light, mass=5}.
 * Selecting a variant changes mass; color survives from base (field merge).
 * base.flux is never modified (non-destructive). */
#include "fluxmeme/fluxmeme.h"
#include "../src/core/ref_utils.h"
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

static const uint8_t FIXED_ID[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

/* write a link layer with the shared id; mass always, color only if non-null,
 * variant tags only if set/variant non-null */
static void put_layer(const char* path, const char* mass, const char* color,
                      const char* set, const char* variant) {
    flux_store_t* s = NULL;
    flux_open(path, 1, &s);
    flux_txn_t* w = NULL;
    flux_txn_begin_write(s, &w);
    flux_meta_kv_t m[5];
    int n = 0;
    m[n].key = "name"; m[n].val = "base"; m[n].type = FLUX_META_STRING; n++;
    m[n].key = "mass"; m[n].val = mass;  m[n].type = FLUX_META_STRING; n++;
    if (color)   { m[n].key = "color"; m[n].val = color; m[n].type = FLUX_META_STRING; n++; }
    if (set)     { m[n].key = "flux_variant_set"; m[n].val = set; m[n].type = FLUX_META_STRING; n++; }
    if (variant) { m[n].key = "flux_variant"; m[n].val = variant; m[n].type = FLUX_META_STRING; n++; }
    flux_record_t r;
    memset(&r, 0, sizeof(r));
    memcpy(r.id.bytes, FIXED_ID, 16);
    r.layer = FLUX_LAYER_BODY;
    r.kind = "robot/link";
    r.meta = m;
    r.meta_count = (uint32_t)n;
    flux_put(w, &r);
    flux_txn_commit(w);
    flux_close(s);
}

static const char* field_of(flux_record_t* r, const char* key) {
    for (uint32_t i = 0; i < r->meta_count; ++i)
        if (r->meta[i].key && strcmp(r->meta[i].key, key) == 0) return r->meta[i].val;
    return "";
}

static const char* resolved_mass(flux_compose_t* c) {
    flux_id_t id;
    memcpy(id.bytes, FIXED_ID, 16);
    flux_record_t r;
    static char out[16];
    out[0] = '\0';
    if (flux_compose_get(c, &id, &r) == FLUX_OK) {
        strncpy(out, field_of(&r, "mass"), sizeof(out) - 1);
        flux_record_free(&r);
    }
    return out;
}
static const char* resolved_color(flux_compose_t* c) {
    flux_id_t id;
    memcpy(id.bytes, FIXED_ID, 16);
    flux_record_t r;
    static char out[16];
    out[0] = '\0';
    if (flux_compose_get(c, &id, &r) == FLUX_OK) {
        strncpy(out, field_of(&r, "color"), sizeof(out) - 1);
        flux_record_free(&r);
    }
    return out;
}

int main(void) {
    put_layer("livrps_base.flux", "10", "red", NULL, NULL);
    put_layer("livrps_heavy.flux", "50", NULL, "config", "heavy");
    put_layer("livrps_light.flux", "5", NULL, "config", "light");

    /* root manifest: heavy, light, base (strongest first) */
    flux_store_t* root = NULL;
    CHECK(flux_open("livrps_root.flux", 1, &root) == FLUX_OK, "open root");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(root, &w) == FLUX_OK, "begin write root");
    flux_meta_kv_t m;
    m.key = "sublayers";
    m.val = "livrps_heavy.flux;livrps_light.flux;livrps_base.flux";
    m.type = FLUX_META_STRING;
    flux_record_t mr;
    memset(&mr, 0, sizeof(mr));
    mr.layer = FLUX_LAYER_MIND;
    mr.kind = "flux/compose";
    mr.meta = &m;
    mr.meta_count = 1;
    CHECK(flux_put(w, &mr) == FLUX_OK, "put manifest");
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit root");
    flux_close(root);

    flux_compose_t* c = NULL;
    CHECK(flux_compose_open("livrps_root.flux", &c) == FLUX_OK, "compose_open");

    /* default (no variant): base only active */
    CHECK(strcmp(resolved_mass(c), "10") == 0, "default mass=10 (base)");
    CHECK(strcmp(resolved_color(c), "red") == 0, "default color=red (base)");

    /* select heavy: mass overridden to 50, color survives from base (field merge) */
    flux_compose_set_variant(c, "config", "heavy");
    CHECK(strcmp(resolved_mass(c), "50") == 0, "heavy variant mass=50");
    CHECK(strcmp(resolved_color(c), "red") == 0, "color survives from base (field merge)");

    /* select light: mass=5 */
    flux_compose_set_variant(c, "config", "light");
    CHECK(strcmp(resolved_mass(c), "5") == 0, "light variant mass=5");
    CHECK(strcmp(resolved_color(c), "red") == 0, "color still red");

    /* non-destructive: base unchanged */
    flux_store_t* b = NULL;
    CHECK(flux_open("livrps_base.flux", 0, &b) == FLUX_OK, "reopen base");
    flux_txn_t* bt = NULL;
    flux_txn_begin_read(b, &bt);
    flux_id_t id;
    memcpy(id.bytes, FIXED_ID, 16);
    flux_record_t br;
    CHECK(flux_get(bt, &id, &br) == FLUX_OK, "get base");
    CHECK(strcmp(field_of(&br, "mass"), "10") == 0, "base mass still 10 (untouched)");
    flux_record_free(&br);
    flux_txn_rollback(bt);
    flux_close(b);

    flux_compose_close(c);
    printf("PASS: demo_livrps field-level merge + variant selection (heavy/light, color survives) OK\n");
    return 0;
}
