/* test_record_codec — encode then decode a record, assert field round-trip. */
#include "../src/core/internal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    flux_meta_kv_t meta[2] = { {"title", "Cube"}, {"tags", "x,y"} };
    flux_link_t link = { {0}, "references" };
    memset(link.target.bytes, 0xAB, 16);
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
    in.meta_count = 2;
    in.links = &link;
    in.link_count = 1;
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
    ok &= out.meta_count == 2;
    ok &= out.meta[0].key && strcmp(out.meta[0].key, "title") == 0;
    ok &= out.meta[0].val && strcmp(out.meta[0].val, "Cube") == 0;
    ok &= out.link_count == 1;
    ok &= memcmp(out.links[0].target.bytes, link.target.bytes, 16) == 0;
    ok &= out.links[0].rel && strcmp(out.links[0].rel, "references") == 0;
    ok &= out.payload.len == strlen(payload);
    ok &= memcmp(out.payload.data, payload, out.payload.len) == 0;
    ok &= out.ts == 1234567ULL;
    ok &= out.ver == 42;

    flux_record_free(&out);
    if (!ok) { printf("FAIL: field mismatch\n"); return 1; }
    printf("PASS\n");
    return 0;
}
