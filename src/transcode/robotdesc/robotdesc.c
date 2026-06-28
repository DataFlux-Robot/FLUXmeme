/* Robot description import — URDF / SDF XML -> BODY robot-graph records.
 *
 * Both URDF (<robot>) and SDF (<model>) use the same <link name=> and
 * <joint name= type=><parent link=/><child link=/></joint> elements, so one
 * scanner handles both. URDF is a tree; closed loops can be added afterward via
 * robot/constraint. See SPEC §6C. */
#include "fluxmeme/fluxmeme.h"
#include "../../core/ref_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

static int boundary(char c) { return isspace((unsigned char)c) || c == '/' || c == '>' || c == '\0'; }

/* find "<name" with a boundary char after name; return ptr at '<' or NULL */
static const char* find_open_tag(const char* p, const char* name) {
    size_t nl = strlen(name);
    for (const char* s = p; (s = strstr(s, "<")) != NULL; ++s) {
        ++s; /* past '<' */
        if (strncmp(s, name, nl) == 0 && boundary(s[nl]))
            return s - 1;
    }
    return NULL;
}

/* find the '>' closing the opening tag starting at '<' */
static const char* tag_close(const char* tag) {
    const char* q = strchr(tag, '>');
    return q;
}

/* extract attr="value" within [tag, tag_end) -> out; return 1 on success */
static int get_attr(const char* tag, const char* tag_end, const char* attr,
                    char* out, size_t outsz) {
    size_t al = strlen(attr);
    for (const char* s = tag; s + al + 2 < tag_end; ++s) {
        if (strncmp(s, attr, al) == 0 && s[al] == '=' && s[al + 1] == '"') {
            const char* v = s + al + 2;
            const char* e = strchr(v, '"');
            if (!e || e > tag_end) return 0;
            size_t n = (size_t)(e - v);
            if (n >= outsz) n = outsz - 1;
            memcpy(out, v, n);
            out[n] = '\0';
            return 1;
        }
    }
    return 0;
}

/* name -> id map */
typedef struct { char name[64]; flux_id_t id; } name_id_t;

static flux_id_t* map_get(name_id_t* m, size_t n, const char* name) {
    for (size_t i = 0; i < n; ++i)
        if (strcmp(m[i].name, name) == 0) return &m[i].id;
    return NULL;
}

static void put_link(flux_txn_t* txn, const char* name, flux_id_t* id_out) {
    flux_meta_kv_t mk = {"name", name, FLUX_META_STRING};
    flux_record_t r;
    memset(&r, 0, sizeof(r));
    r.layer = FLUX_LAYER_BODY;
    r.kind = "robot/link";
    r.meta = &mk;
    r.meta_count = 1;
    r.ts = (uint64_t)time(NULL) * 1000ULL;
    flux_put(txn, &r);
    *id_out = r.id;
}

static void put_joint(flux_txn_t* txn, const char* type,
                      const flux_id_t* parent, const flux_id_t* child) {
    const char* t = (type && *type) ? type : "fixed";
    char phex[33]; flux_id_to_hex(parent, phex);
    char chex[33]; flux_id_to_hex(child, chex);
    char pval[80]; flux_ref_encode(pval, sizeof(pval), phex, "mechanical");
    char cval[80]; flux_ref_encode(cval, sizeof(cval), chex, "mechanical");
    flux_meta_kv_t mk[3];
    mk[0].key = "type";   mk[0].val = t;    mk[0].type = FLUX_META_STRING;
    mk[1].key = "parent"; mk[1].val = pval; mk[1].type = FLUX_META_REF;
    mk[2].key = "child";  mk[2].val = cval; mk[2].type = FLUX_META_REF;
    flux_record_t r;
    memset(&r, 0, sizeof(r));
    r.layer = FLUX_LAYER_BODY;
    r.kind = "robot/joint";
    r.meta = mk;
    r.meta_count = 3;
    r.ts = (uint64_t)time(NULL) * 1000ULL;
    flux_put(txn, &r);
}

/* shared importer for URDF and SDF (same <link>/<joint> shape) */
static flux_status_t import_xml(const char* in_path, flux_txn_t* txn) {
    FILE* f = fopen(in_path, "rb");
    if (!f) return FLUX_ERR_IO;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* src = (char*)malloc((size_t)sz + 1);
    if (!src) { fclose(f); return FLUX_ERR_NOMEM; }
    size_t total = fread(src, 1, (size_t)sz, f);
    src[total] = '\0';
    fclose(f);

    name_id_t* map = NULL;
    size_t nmap = 0;

    /* pass 1: links */
    const char* p = src;
    while ((p = find_open_tag(p, "link")) != NULL) {
        const char* tc = tag_close(p);
        if (!tc) break;
        char name[64];
        if (get_attr(p, tc, "name", name, sizeof(name)) && *name) {
            map = realloc(map, (nmap + 1) * sizeof(name_id_t));
            strncpy(map[nmap].name, name, sizeof(map[nmap].name) - 1);
            map[nmap].name[sizeof(map[nmap].name) - 1] = '\0';
            put_link(txn, name, &map[nmap].id);
            nmap++;
        }
        p = tc + 1;
    }

    /* pass 2: joints (with dynamics: limit + axis extracted into meta) */
    p = src;
    while ((p = find_open_tag(p, "joint")) != NULL) {
        const char* tc = tag_close(p);
        if (!tc) break;
        char jtype[32] = {0};
        get_attr(p, tc, "type", jtype, sizeof(jtype));
        const char* jend = strstr(p, "</joint>");
        if (!jend) jend = src + total;
        char pname[64] = {0}, cname[64] = {0};
        const char* pt = find_open_tag(p, "parent");
        if (pt && pt < jend) { const char* ptc = tag_close(pt); if (ptc) get_attr(pt, ptc, "link", pname, sizeof(pname)); }
        const char* ct = find_open_tag(p, "child");
        if (ct && ct < jend) { const char* ctc = tag_close(ct); if (ctc) get_attr(ct, ctc, "link", cname, sizeof(cname)); }
        flux_id_t* par = map_get(map, nmap, pname);
        flux_id_t* chd = map_get(map, nmap, cname);
        if (!par || !chd) { p = jend + 1; continue; }

        /* dynamics: extract <limit lower/upper/effort/velocity> + <axis xyz> */
        flux_meta_kv_t jm[8];
        int jn = 0;
        jm[jn].key = "type"; jm[jn].val = (jtype[0] ? jtype : "fixed");
        jm[jn].type = FLUX_META_STRING; jn++;
        char lo[32]={0}, hi[32]={0}, eff[32]={0}, vel[32]={0}, ax[64]={0};
        const char* lt = find_open_tag(p, "limit");
        if (lt && lt < jend) {
            const char* ltc = tag_close(lt);
            if (ltc) {
                get_attr(lt, ltc, "lower", lo, sizeof(lo));
                get_attr(lt, ltc, "upper", hi, sizeof(hi));
                get_attr(lt, ltc, "effort", eff, sizeof(eff));
                get_attr(lt, ltc, "velocity", vel, sizeof(vel));
            }
        }
        const char* at = find_open_tag(p, "axis");
        if (at && at < jend) {
            const char* atc = tag_close(at);
            if (atc) get_attr(at, atc, "xyz", ax, sizeof(ax));
        }
        if (lo[0])  { jm[jn].key="lower";   jm[jn].val=lo;  jm[jn].type=FLUX_META_FLOAT; jn++; }
        if (hi[0])  { jm[jn].key="upper";   jm[jn].val=hi;  jm[jn].type=FLUX_META_FLOAT; jn++; }
        if (eff[0]) { jm[jn].key="effort";  jm[jn].val=eff; jm[jn].type=FLUX_META_FLOAT; jn++; }
        if (vel[0]) { jm[jn].key="velocity";jm[jn].val=vel; jm[jn].type=FLUX_META_FLOAT; jn++; }
        if (ax[0])  { jm[jn].key="axis";    jm[jn].val=ax;  jm[jn].type=FLUX_META_STRING; jn++; }

        /* parent/child as REF-typed meta entries */
        char phex[33]; flux_id_to_hex(par, phex);
        char chex[33]; flux_id_to_hex(chd, chex);
        char pval[80]; flux_ref_encode(pval, sizeof(pval), phex, "mechanical");
        char cval[80]; flux_ref_encode(cval, sizeof(cval), chex, "mechanical");
        jm[jn].key="parent"; jm[jn].val=pval; jm[jn].type=FLUX_META_REF; jn++;
        jm[jn].key="child";  jm[jn].val=cval; jm[jn].type=FLUX_META_REF; jn++;

        /* build the joint record with parent/child + dynamics meta */
        flux_record_t r;
        memset(&r, 0, sizeof(r));
        r.layer = FLUX_LAYER_BODY;
        r.kind = "robot/joint";
        r.meta = jm;
        r.meta_count = (uint32_t)jn;
        r.ts = (uint64_t)time(NULL) * 1000ULL;
        flux_put(txn, &r);
        p = jend + 1;
    }

    free(map);
    free(src);
    return FLUX_OK;
}

flux_status_t flux_from_urdf(const char* in_urdf, flux_txn_t* txn) {
    if (!in_urdf || !txn) return FLUX_ERR_ARG;
    return import_xml(in_urdf, txn);
}

flux_status_t flux_from_sdf(const char* in_sdf, flux_txn_t* txn) {
    if (!in_sdf || !txn) return FLUX_ERR_ARG;
    return import_xml(in_sdf, txn);
}
