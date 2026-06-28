/* demo_a2a — store -> A2A JSON bundle -> store round-trip.
 * Writes an agent_card (name=fluxbot) + 2 tasks (part_of the card),
 * transcodes out to a2a_out/, re-ingests, asserts the round-trip. */
#include "fluxmeme/fluxmeme.h"
#include "../src/core/ref_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHECK(cond, msg)                                                       \
    do { if (!(cond)) { printf("FAIL: %s (%s)\n", msg, flux_last_error()); return 1; } } while (0)

int main(void) {
    flux_store_t* s = NULL;
    CHECK(flux_open("demo_a2a.flux", 1, &s) == FLUX_OK, "open store");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(s, &w) == FLUX_OK, "begin write");

    /* agent_card: stable id so it round-trips */
    flux_record_t card;
    memset(&card, 0, sizeof(card));
    memset(card.id.bytes, 0xC0, 16);
    card.layer = FLUX_LAYER_MIND;
    card.pclass = FLUX_PCLASS_TEXT;
    card.kind = "agent_card";
    card.ptype = "application/json";
    const char* card_json = "{\"name\":\"fluxbot\",\"version\":\"1.0\"}";
    card.payload.data = (const uint8_t*)card_json;
    card.payload.len = strlen(card_json);
    CHECK(flux_put(w, &card) == FLUX_OK, "put agent_card");

    /* two tasks linked to the card (part_of) */
    for (int i = 0; i < 2; ++i) {
        flux_record_t t;
        memset(&t, 0, sizeof(t));
        t.layer = FLUX_LAYER_MIND;
        t.pclass = FLUX_PCLASS_TEXT;
        t.kind = "task";
        t.ptype = "application/json";
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"task\":\"t%d\"}", i + 1);
        t.payload.data = (const uint8_t*)buf;
        t.payload.len = strlen(buf);
        /* link task -> card (part_of) as REF-typed meta */
        static char hexbuf[33];
        static char refbuf[80];
        static flux_meta_kv_t tmeta[1];
        flux_id_to_hex(&card.id, hexbuf);
        flux_ref_encode(refbuf, sizeof(refbuf), hexbuf, NULL);
        tmeta[0].key = "part_of"; tmeta[0].val = refbuf; tmeta[0].type = FLUX_META_REF;
        t.meta = tmeta; t.meta_count = 1;
        CHECK(flux_put(w, &t) == FLUX_OK, "put task");
    }
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit");

    /* transcode out */
    flux_txn_t* r1 = NULL;
    CHECK(flux_txn_begin_read(s, &r1) == FLUX_OK, "begin read");
    CHECK(flux_to_a2a(r1, "a2a_out") == FLUX_OK, "to_a2a");

    /* ingest into a fresh store */
    flux_store_t* s2 = NULL;
    CHECK(flux_open("demo_a2a2.flux", 1, &s2) == FLUX_OK, "open store2");
    flux_txn_t* w2 = NULL;
    CHECK(flux_txn_begin_write(s2, &w2) == FLUX_OK, "begin write2");
    CHECK(flux_from_a2a("a2a_out", w2) == FLUX_OK, "from_a2a");
    CHECK(flux_txn_commit(w2) == FLUX_OK, "commit2");

    /* verify */
    flux_txn_t* r2 = NULL;
    CHECK(flux_txn_begin_read(s2, &r2) == FLUX_OK, "begin read2");
    flux_iter_t* it = NULL;
    CHECK(flux_scan(r2, NULL, &it) == FLUX_OK, "scan");
    int n_card = 0, n_task = 0, link_ok = 0;
    flux_record_t rec;
    while (flux_iter_next(it, &rec) == FLUX_OK) {
        if (rec.kind && strcmp(rec.kind, "agent_card") == 0) {
            n_card++;
            /* payload should contain "fluxbot" */
            if (rec.payload.len >= 8 && strstr((const char*)rec.payload.data, "fluxbot"))
                n_card++;
        } else if (rec.kind && strcmp(rec.kind, "task") == 0) {
            n_task++;
            if (flux_ref_count(&rec) >= 1) link_ok++;
        }
        flux_record_free(&rec);
    }
    flux_iter_free(it);

    CHECK(n_card == 2, "agent_card recovered + name=fluxbot"); /* n_card incremented twice */
    CHECK(n_task == 2, "2 tasks recovered");
    CHECK(link_ok == 2, "tasks link to card (part_of)");

    flux_close(s2);
    flux_close(s);
    printf("PASS: demo_a2a round-trip (agent_card + 2 tasks, part_of links) OK\n");
    return 0;
}
