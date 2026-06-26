/* USD transcoder — BODY <-> USDA (minimal, round-trips what we emit).
 *   out : kind=mesh BIN(model/stl) -> def Mesh(points/faceVertexCounts/indices)
 *         kind=usda_snippet TEXT    -> spliced verbatim
 *   in  : def Mesh {points,faceVertexCounts,faceVertexIndices} -> STL -> BODY/mesh
 * See SPEC §1.7 (USD is a fast transcode target, not a superset). */
#include "fluxmeme/fluxmeme.h"
#include "stl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

/* ---------- emit ---------- */
static void emit_mesh(FILE* f, const char* hex, const flux_stl_tri* tris, uint32_t n) {
    /* non-indexed: 3 verts per triangle, indices 0..3n-1 */
    fprintf(f, "    def Mesh \"mesh_%s\"\n    {\n", hex);
    fprintf(f, "        int[] faceVertexCounts = [");
    for (uint32_t i = 0; i < n; ++i) fprintf(f, "%s3", i ? ", " : "");
    fprintf(f, "]\n");
    fprintf(f, "        int[] faceVertexIndices = [");
    for (uint32_t i = 0; i < n * 3; ++i) fprintf(f, "%s%u", i ? ", " : "", i);
    fprintf(f, "]\n");
    fprintf(f, "        point3f[] points = [");
    for (uint32_t i = 0; i < n; ++i)
        for (int j = 0; j < 3; ++j) {
            int idx = (i * 3 + j);
            fprintf(f, "%s(%.7g, %.7g, %.7g)", idx ? ", " : "",
                    tris[i].v[j][0], tris[i].v[j][1], tris[i].v[j][2]);
        }
    fprintf(f, "]\n");
    fprintf(f, "    }\n");
}

flux_status_t flux_to_usd(const flux_txn_t* txn, const char* out_usda) {
    if (!txn || !out_usda) return FLUX_ERR_ARG;
    FILE* f = fopen(out_usda, "wb");
    if (!f) return FLUX_ERR_IO;
    fprintf(f, "#usda 1.0\n");
    fprintf(f, "def Xform \"Body\"\n{\n");

    flux_iter_t* it = NULL;
    if (flux_scan(txn, NULL, &it) != FLUX_OK) { fclose(f); return FLUX_ERR_IO; }
    flux_record_t rec;
    while (flux_iter_next(it, &rec) == FLUX_OK) {
        if (!(rec.layer & FLUX_LAYER_BODY)) { flux_record_free(&rec); continue; }
        char hex[33];
        flux_id_to_hex(&rec.id, hex);
        if (rec.kind && strcmp(rec.kind, "mesh") == 0 && rec.pclass == FLUX_PCLASS_BIN &&
            rec.payload.len) {
            flux_stl_tri* tris = NULL;
            uint32_t n = 0;
            if (flux_stl_decode(rec.payload.data, rec.payload.len, &tris, &n) == 0 && n > 0) {
                emit_mesh(f, hex, tris, n);
                free(tris);
            }
        } else if (rec.kind && strcmp(rec.kind, "usda_snippet") == 0 &&
                   rec.pclass == FLUX_PCLASS_TEXT && rec.payload.len) {
            fwrite(rec.payload.data, 1, rec.payload.len, f);
            fputc('\n', f);
        } else {
            fprintf(f, "    def Scope \"%s_%s\" {}\n", rec.kind ? rec.kind : "record", hex);
        }
        flux_record_free(&rec);
    }
    flux_iter_free(it);
    fprintf(f, "}\n");
    fclose(f);
    return FLUX_OK;
}

/* ---------- parse (in) ---------- */
static const char* parse_floats_until(const char* p, char term, float** out, size_t* n) {
    float* arr = NULL; size_t cap = 0, cnt = 0;
    while (*p && *p != term) {
        char* end;
        float v = strtof(p, &end);
        if (end == p) { p++; continue; }
        if (cnt == cap) { cap = cap ? cap * 2 : 16; arr = realloc(arr, cap * sizeof(float)); }
        arr[cnt++] = v;
        p = end;
    }
    *out = arr; *n = cnt;
    return p;
}

static const char* parse_ints_until(const char* p, char term, int** out, size_t* n) {
    int* arr = NULL; size_t cap = 0, cnt = 0;
    while (*p && *p != term) {
        char* end;
        long v = strtol(p, &end, 10);
        if (end == p) { p++; continue; }
        if (cnt == cap) { cap = cap ? cap * 2 : 16; arr = realloc(arr, cap * sizeof(int)); }
        arr[cnt++] = (int)v;
        p = end;
    }
    *out = arr; *n = cnt;
    return p;
}

/* find the value list after `name =` within block [blk, blk_end); return ptr at '[' or NULL */
static const char* find_array(const char* blk, const char* blk_end, const char* name) {
    size_t nl = strlen(name);
    const char* p = blk;
    while (p < blk_end) {
        if (strncmp(p, name, nl) == 0) {
            const char* q = p + nl;
            while (q < blk_end && (*q == ' ' || *q == '\t')) q++;
            if (q < blk_end && *q == '=') {
                q++;
                while (q < blk_end && (*q == ' ' || *q == '\t')) q++;
                if (q < blk_end && *q == '[') return q;
            }
        }
        p++;
    }
    return NULL;
}

static void put_mesh_record(flux_txn_t* txn, const flux_stl_tri* tris, uint32_t n) {
    uint8_t* stl = NULL;
    size_t stl_len = 0;
    if (flux_stl_encode(tris, n, &stl, &stl_len) != 0) return;
    flux_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.layer = FLUX_LAYER_BODY;
    rec.pclass = FLUX_PCLASS_BIN;
    rec.kind = "mesh";
    rec.ptype = "model/stl";
    rec.payload.data = stl;
    rec.payload.len = stl_len;
    rec.ts = (uint64_t)time(NULL) * 1000ULL;
    flux_put(txn, &rec);
    free(stl);
}

flux_status_t flux_from_usd(const char* in_usda, flux_txn_t* txn) {
    if (!in_usda || !txn) return FLUX_ERR_ARG;
    FILE* f = fopen(in_usda, "rb");
    if (!f) return FLUX_ERR_IO;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* src = (char*)malloc((size_t)sz + 1);
    if (!src) { fclose(f); return FLUX_ERR_NOMEM; }
    size_t rd = fread(src, 1, (size_t)sz, f);
    src[rd] = '\0';
    fclose(f);

    /* walk every "def Mesh" ... matching brace */
    const char* p = src;
    int imported = 0;
    while ((p = strstr(p, "def Mesh")) != NULL) {
        const char* brace = strchr(p, '{');
        if (!brace) break;
        /* find matching close brace (naive: no nested def assumed) */
        const char* end = strchr(brace, '}');
        if (!end) break;

        const char* blk = brace + 1;
        const char* blk_end = end;

        float* pts = NULL; size_t npts = 0;
        int* counts = NULL; size_t ncounts = 0;
        int* idx = NULL; size_t nidx = 0;

        const char* a;
        if ((a = find_array(blk, blk_end, "points")) != NULL)
            parse_floats_until(a + 1, ']', &pts, &npts);
        if ((a = find_array(blk, blk_end, "faceVertexCounts")) != NULL)
            parse_ints_until(a + 1, ']', &counts, &ncounts);
        if ((a = find_array(blk, blk_end, "faceVertexIndices")) != NULL)
            parse_ints_until(a + 1, ']', &idx, &nidx);

        if (npts >= 3 && ncounts >= 1 && idx) {
            /* reconstruct triangles: walk faces (triangle fans of counts[i]) */
            flux_stl_tri* tris = (flux_stl_tri*)malloc(sizeof(flux_stl_tri) * ncounts);
            uint32_t nt = 0;
            size_t base = 0;
            for (size_t i = 0; i < ncounts && base + 2 < nidx; ++i) {
                if (counts[i] == 3 && base + 2 < nidx) {
                    for (int j = 0; j < 3; ++j) {
                        int vi = idx[base + j];
                        if ((size_t)vi * 3 + 2 < npts) {
                            tris[nt].v[j][0] = pts[vi * 3];
                            tris[nt].v[j][1] = pts[vi * 3 + 1];
                            tris[nt].v[j][2] = pts[vi * 3 + 2];
                        }
                        tris[nt].n[j] = 0.f; /* normal recomputed/ignored */
                    }
                    nt++;
                }
                base += counts[i];
            }
            if (nt > 0) { put_mesh_record(txn, tris, nt); imported++; }
            free(tris);
        }
        free(pts); free(counts); free(idx);
        p = end + 1;
    }
    free(src);
    return imported > 0 ? FLUX_OK : FLUX_OK;
}
