/* A2A in: ingest an A2A-style bundle into the store as MIND records.
 *   agent-card.json -> MIND/kind=agent_card ; tasks/*.json -> MIND/kind=task.
 * Tasks are auto-linked `part_of` the card (an A2A bundle's tasks belong to its
 * agent). Reverses a2a_out. MSVC-safe (no nested functions). */
#include "fluxmeme/fluxmeme.h"
#include "../fsutil.h"
#include "../../core/ref_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static char* read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    *out_len = rd;
    return buf;
}

static int has_suffix(const char* s, const char* suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

/* put one json file as a MIND record; tasks get a `part_of` link to card_id */
static void put_one(const char* dir, const char* name, const char* kind,
                    flux_txn_t* txn, const flux_id_t* card_id) {
    flux_id_t id;
    memset(&id, 0, sizeof(id));
    if (strcmp(name, "agent-card.json") == 0) {
        memset(id.bytes, 0xC0, 16); /* stable id for the card */
    } else {
        char hex[33];
        size_t hl = strlen(name) - 5; /* drop ".json" */
        if (hl != 32) return;
        memcpy(hex, name, 32);
        hex[32] = '\0';
        if (flux_id_from_hex(hex, &id) != FLUX_OK) return;
    }
    char path[1100];
    snprintf(path, sizeof(path), "%s%c%s", dir, FLUX_PATH_SEP, name);
    size_t len = 0;
    char* content = read_file(path, &len);
    if (!content) return;

    /* tasks get a `part_of` REF connection back to the agent card */
    flux_meta_kv_t ref_meta;
    char ref_val[80];
    int is_task = (card_id && strcmp(kind, "task") == 0);
    if (is_task) {
        char chex[33]; flux_id_to_hex(card_id, chex);
        flux_ref_encode(ref_val, sizeof(ref_val), chex, "");
        ref_meta.key = "part_of";
        ref_meta.val = ref_val;
        ref_meta.type = FLUX_META_REF;
    }

    flux_record_t rec;
    memset(&rec, 0, sizeof(rec));
    memcpy(rec.id.bytes, id.bytes, 16);
    rec.layer = FLUX_LAYER_MIND;
    rec.pclass = FLUX_PCLASS_TEXT;
    rec.kind = kind;
    rec.ptype = "application/json";
    if (is_task) {
        rec.meta = &ref_meta;
        rec.meta_count = 1;
    }
    rec.payload.data = (const uint8_t*)content;
    rec.payload.len = len;
    rec.ts = (uint64_t)time(NULL) * 1000ULL;
    flux_put(txn, &rec);
    free(content);
}

typedef struct { const char* dir; const char* kind; flux_txn_t* txn; const flux_id_t* card_id; } a2a_ctx;

static void ingest_cb(const char* name, void* ud) {
    a2a_ctx* c = (a2a_ctx*)ud;
    if (!has_suffix(name, ".json")) return;
    put_one(c->dir, name, c->kind, c->txn, c->card_id);
}

flux_status_t flux_from_a2a(const char* in_dir, flux_txn_t* txn) {
    if (!in_dir || !txn) return FLUX_ERR_ARG;

    flux_id_t card_id;
    memset(card_id.bytes, 0xC0, 16);

    /* agent-card.json (if present) */
    char card[1100];
    snprintf(card, sizeof(card), "%s%cagent-card.json", in_dir, FLUX_PATH_SEP);
    FILE* tst = fopen(card, "rb");
    if (tst) { fclose(tst); put_one(in_dir, "agent-card.json", "agent_card", txn, NULL); }

    /* tasks/*.json — auto-linked part_of the card */
    char tdir[1100];
    snprintf(tdir, sizeof(tdir), "%s%ctasks", in_dir, FLUX_PATH_SEP);
    a2a_ctx ctx = { tdir, "task", txn, &card_id };
    flux_list_dir(tdir, ingest_cb, &ctx);
    return FLUX_OK;
}
