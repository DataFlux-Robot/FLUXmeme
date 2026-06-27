/* demo_lifecycle (★hero) — one .flux through "generate -> reuse -> operate".
 * Desktop slice of the lifecycle narrative:
 *   1. GENERATE a DevReady .flux: BODY (robot graph) + MIND (concept + agent
 *      card) + JOURNAL (an initial PHM signal) in a single store.
 *   2. REUSE: project BODY -> USD, MIND -> OKF (multi-projection from one source).
 *   3. OPERATE: append a second PHM signal (the journal grows on-device-style),
 *      then read the unified twin — body + mind + the grown journal coexist.
 * (The MCU on-device append is the v1.3 embedded piece; this runs the desktop
 *  half, asserting the single-artifact-through-lifecycle property.) */
#include "fluxmeme/fluxmeme.h"
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

static int count_layer(flux_store_t* s, int mask) {
    flux_txn_t* t = NULL;
    flux_txn_begin_read(s, &t);
    flux_iter_t* it = NULL;
    flux_scan(t, NULL, &it);
    int n = 0;
    flux_record_t r;
    while (flux_iter_next(it, &r) == FLUX_OK) {
        if (r.layer & mask) n++;
        flux_record_free(&r);
    }
    flux_iter_free(it);
    flux_txn_rollback(t);
    return n;
}

int main(void) {
    /* 1. GENERATE: body + mind + journal in one store */
    flux_store_t* s = NULL;
    CHECK(flux_open("lifecycle.flux", 1, &s) == FLUX_OK, "open");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(s, &w) == FLUX_OK, "begin write");

    /* BODY: a tiny robot graph (2 links + 1 joint) */
    flux_meta_kv_t ln = {"name", "base"};
    flux_record_t link = {0};
    link.layer = FLUX_LAYER_BODY; link.kind = "robot/link"; link.meta = &ln; link.meta_count = 1;
    CHECK(flux_put(w, &link) == FLUX_OK, "put base link");
    flux_id_t base = link.id;
    flux_meta_kv_t ln2 = {"name", "arm"};
    flux_record_t link2 = {0};
    link2.layer = FLUX_LAYER_BODY; link2.kind = "robot/link"; link2.meta = &ln2; link2.meta_count = 1;
    CHECK(flux_put(w, &link2) == FLUX_OK, "put arm link");
    flux_id_t arm = link2.id;
    flux_link_t jl = {0};
    memcpy(jl.target.bytes, arm.bytes, 16); jl.rel = "child";
    static const char* rp = "parent";
    flux_link_t jpar = {0};
    memcpy(jpar.target.bytes, base.bytes, 16); jpar.rel = rp;
    flux_link_t jlinks[2] = {0};
    jlinks[0] = jpar; jlinks[1] = jl;
    flux_meta_kv_t jt = {"type", "revolute"};
    flux_record_t joint = {0};
    joint.layer = FLUX_LAYER_BODY; joint.kind = "robot/joint"; joint.meta = &jt; joint.meta_count = 1;
    joint.links = jlinks; joint.link_count = 2;
    CHECK(flux_put(w, &joint) == FLUX_OK, "put joint");

    /* MIND: a task concept + an agent card */
    flux_meta_kv_t cm = {"title", "pick"};
    flux_record_t concept = {0};
    concept.layer = FLUX_LAYER_MIND; concept.kind = "concept";
    concept.meta = &cm; concept.meta_count = 1;
    concept.payload.data = (const uint8_t*)"# Pick task"; concept.payload.len = 10;
    CHECK(flux_put(w, &concept) == FLUX_OK, "put concept");
    flux_record_t card = {0};
    card.layer = FLUX_LAYER_MIND; card.kind = "agent_card"; card.ptype = "application/json";
    card.payload.data = (const uint8_t*)"{\"skill\":\"grasp\"}"; card.payload.len = 16;
    CHECK(flux_put(w, &card) == FLUX_OK, "put agent card");

    /* JOURNAL: an initial PHM signal */
    flux_meta_kv_t sm = {"name", "battery"};
    flux_record_t sig = {0};
    sig.layer = FLUX_LAYER_JOURNAL; sig.kind = "signal"; sig.clock = FLUX_CLOCK_DEVICE_MONOTONIC;
    sig.meta = &sm; sig.meta_count = 1;
    CHECK(flux_put(w, &sig) == FLUX_OK, "put initial signal");
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit generate");

    CHECK(count_layer(s, FLUX_LAYER_BODY) == 3, "BODY: 2 links + joint");
    CHECK(count_layer(s, FLUX_LAYER_MIND) == 2, "MIND: concept + card");
    CHECK(count_layer(s, FLUX_LAYER_JOURNAL) == 1, "JOURNAL: 1 signal");

    /* 2. REUSE: project BODY->USD, MIND->OKF from the same source */
    flux_txn_t* r1 = NULL;
    CHECK(flux_txn_begin_read(s, &r1) == FLUX_OK, "begin read");
    CHECK(flux_to_usd(r1, "lifecycle.usda") == FLUX_OK, "to_usd (BODY view)");
    CHECK(flux_to_okf(r1, "lifecycle_okf") == FLUX_OK, "to_okf (MIND view)");
    FILE* fu = fopen("lifecycle.usda", "rb");
    CHECK(fu != NULL, "USDA written"); fclose(fu);
    CHECK(fopen("lifecycle_okf/index.md", "rb") != NULL, "OKF bundle written");
    /* (the fopen result for index.md isn't closed for brevity; it's read-only) */

    /* 3. OPERATE: append a second PHM signal (journal grows, on-device-style) */
    flux_txn_t* w2 = NULL;
    CHECK(flux_txn_begin_write(s, &w2) == FLUX_OK, "begin write operate");
    flux_meta_kv_t sm2 = {"name", "temp"};
    flux_record_t sig2 = {0};
    sig2.layer = FLUX_LAYER_JOURNAL; sig2.kind = "signal"; sig2.clock = FLUX_CLOCK_DEVICE_MONOTONIC;
    sig2.meta = &sm2; sig2.meta_count = 1;
    CHECK(flux_put(w2, &sig2) == FLUX_OK, "append second signal");
    CHECK(flux_txn_commit(w2) == FLUX_OK, "commit operate");

    /* unified twin: body + mind + grown journal coexist in the ONE .flux */
    CHECK(count_layer(s, FLUX_LAYER_BODY) == 3, "BODY intact");
    CHECK(count_layer(s, FLUX_LAYER_MIND) == 2, "MIND intact");
    CHECK(count_layer(s, FLUX_LAYER_JOURNAL) == 2, "JOURNAL grew (1 -> 2)");

    flux_close(s);
    printf("PASS: demo_lifecycle generate->reuse(USD+OKF)->operate(journal grows) in one .flux OK\n");
    return 0;
}
