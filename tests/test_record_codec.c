/* test_record_codec — encode then decode a record, assert field round-trip. */
#include "../src/core/internal.h"
#include "../src/core/ref_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    /* build a REF target id and encode it as "hex@graph" */
    flux_id_t target;
    memset(target.bytes, 0xAB, 16);
    char hex[33];
    flux_id_to_hex(&target, hex);
    char refval[80];
    flux_ref_encode(refval, sizeof(refval), hex, NULL);

    flux_meta_kv_t meta[3] = {
        {"title", "Cube", FLUX_META_STRING},
        {"tags", "x,y", FLUX_META_STRING},
        {"references", refval, FLUX_META_REF},
    };
    const char* payload = "# Hello\nbody";
    flux_record_t in;
    memset(&in, 0, sizeof(in));
    memset(in.id.bytes, 0x42, 16);
    in.layer = FLUX_LAYER_MIND;
    in.pclass = FLUX_PCLASS_TEXT;
    in.kind = "concept";
    in.ptype = "text/markdown";
    in.path = "mind/concepts/cube";
    in.meta = meta;
    in.meta_count = 3;
    in.payload.data = (const uint8_t*)payload;
    in.payload.len = strlen(payload);
    in.ts = 1234567ULL;
    in.ver = 42;

    flux_buf_t body;
    if (flux_record_encode(&in, &body) != FLUX_OK) { printf("FAIL: encode\n"); return 1; }

    flux_record_t out;
    if (flux_record_decode(body.data, body.len, &out) != FLUX_OK) { printf("FAIL: decode\n"); free(body.data); return 1; }
    free(body.data);

    int ok = 1;
    ok &= memcmp(out.id.bytes, in.id.bytes, 16) == 0;
    ok &= out.layer == in.layer;
    ok &= out.pclass == in.pclass;
    ok &= out.kind && strcmp(out.kind, "concept") == 0;
    ok &= out.path && strcmp(out.path, "mind/concepts/cube") == 0;
    ok &= out.meta_count == 3;
    ok &= out.meta[0].key && strcmp(out.meta[0].key, "title") == 0;
    ok &= out.meta[0].val && strcmp(out.meta[0].val, "Cube") == 0;
    /* v2: connections are REF-typed meta */
    ok &= flux_ref_count(&out) == 1;
    char outhex[33]; const char* outrel; const char* outgraph;
    ok &= flux_ref_at(&out, 0, outhex, &outrel, &outgraph) == FLUX_OK;
    ok &= outrel && strcmp(outrel, "references") == 0;
    ok &= outgraph && strcmp(outgraph, "") == 0;
    flux_id_t outtarget;
    ok &= flux_id_from_hex(outhex, &outtarget) == FLUX_OK;
    ok &= memcmp(outtarget.bytes, target.bytes, 16) == 0;
    ok &= out.payload.len == strlen(payload);
    ok &= memcmp(out.payload.data, payload, out.payload.len) == 0;
    ok &= out.ts == 1234567ULL;
    ok &= out.ver == 42;

    flux_record_free(&out);
    if (!ok) { printf("FAIL: field mismatch\n"); return 1; }
    printf("PASS\n");
    return 0;
}
