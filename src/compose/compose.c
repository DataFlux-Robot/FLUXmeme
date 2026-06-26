/* composition — read-time merged view over a layer stack. */
#include "compose.h"
#include <stdlib.h>
#include <string.h>

struct flux_compose {
    flux_store_t** layers; /* [0]=root, then sublayers strongest-first */
    size_t n_layers;
    char* root_dir;
};

/* dirname of a path (malloc'd, no trailing sep). */
static char* path_dirname(const char* p) {
    const char* slash = strrchr(p, '/');
    const char* bslash = strrchr(p, '\\');
    const char* sep = (bslash && (!slash || bslash > slash)) ? bslash : slash;
    if (!sep) return strdup(".");
    if (sep == p) return strdup("/");
    size_t n = (size_t)(sep - p);
    char* d = (char*)malloc(n + 1);
    memcpy(d, p, n);
    d[n] = '\0';
    return d;
}

static char* join(const char* dir, const char* rel) {
    size_t ld = strlen(dir), lr = strlen(rel);
    char* s = (char*)malloc(ld + 1 + lr + 1);
    memcpy(s, dir, ld);
    s[ld] = '/';
    memcpy(s + ld + 1, rel, lr);
    s[ld + 1 + lr] = '\0';
    return s;
}

static const char* meta_val(const flux_record_t* r, const char* key) {
    for (uint32_t i = 0; i < r->meta_count; ++i)
        if (r->meta[i].key && strcmp(r->meta[i].key, key) == 0)
            return r->meta[i].val ? r->meta[i].val : "";
    return "";
}

/* pull the ';'-separated sublayers list from the root's flux/compose record */
static flux_status_t read_sublayers(flux_store_t* root, char** out_list) {
    *out_list = NULL;
    flux_txn_t* t = NULL;
    if (flux_txn_begin_read(root, &t) != FLUX_OK) return FLUX_ERR_IO;
    flux_iter_t* it = NULL;
    flux_scan(t, NULL, &it);
    flux_record_t r;
    while (flux_iter_next(it, &r) == FLUX_OK) {
        if (r.kind && strcmp(r.kind, "flux/compose") == 0) {
            const char* v = meta_val(&r, "sublayers");
            if (*v) *out_list = strdup(v);
            flux_record_free(&r);
            break;
        }
        flux_record_free(&r);
    }
    flux_iter_free(it);
    flux_txn_rollback(t); /* free read txn */
    return FLUX_OK;
}

flux_status_t flux_compose_open(const char* root_path, flux_compose_t** out) {
    if (!root_path || !out) return FLUX_ERR_ARG;
    flux_compose_t* c = (flux_compose_t*)calloc(1, sizeof(*c));
    if (!c) return FLUX_ERR_NOMEM;
    c->root_dir = path_dirname(root_path);

    flux_store_t* root = NULL;
    if (flux_open(root_path, 0, &root) != FLUX_OK) { free(c->root_dir); free(c); return FLUX_ERR_IO; }
    c->layers = (flux_store_t**)malloc(sizeof(flux_store_t*));
    c->layers[0] = root;
    c->n_layers = 1;

    char* list = NULL;
    read_sublayers(root, &list);
    if (list) {
        char* tok = strtok(list, ";");
        while (tok) {
            while (*tok == ' ') tok++;
            char* full = join(c->root_dir, tok);
            flux_store_t* lay = NULL;
            if (flux_open(full, 0, &lay) == FLUX_OK) {
                c->layers = realloc(c->layers, (c->n_layers + 1) * sizeof(flux_store_t*));
                c->layers[c->n_layers++] = lay;
            }
            free(full);
            tok = strtok(NULL, ";");
        }
        free(list);
    }
    *out = c;
    return FLUX_OK;
}

void flux_compose_close(flux_compose_t* c) {
    if (!c) return;
    for (size_t i = 0; i < c->n_layers; ++i) flux_close(c->layers[i]);
    free(c->layers);
    free(c->root_dir);
    free(c);
}

size_t flux_compose_n_layers(const flux_compose_t* c) {
    return c ? c->n_layers : 0;
}

flux_status_t flux_compose_get(flux_compose_t* c, const flux_id_t* id, flux_record_t* out) {
    if (!c || !id || !out) return FLUX_ERR_ARG;
    for (size_t i = 0; i < c->n_layers; ++i) {
        flux_txn_t* t = NULL;
        if (flux_txn_begin_read(c->layers[i], &t) != FLUX_OK) continue;
        flux_status_t st = flux_get(t, id, out);
        flux_txn_rollback(t);
        if (st == FLUX_OK) return FLUX_OK; /* strongest layer wins */
    }
    return FLUX_ERR_NOTFOUND;
}

/* ---- merged scan ---- */
struct flux_compose_iter {
    flux_id_t* ids;   /* unique ids, strongest-first assignment stored per id */
    size_t n_ids;
    size_t cur;
    flux_compose_t* c;
    flux_filter_t f;
    int has_filter;
};

static int id_seen(const flux_id_t* ids, size_t n, const flux_id_t* id) {
    for (size_t i = 0; i < n; ++i)
        if (memcmp(ids[i].bytes, id->bytes, 16) == 0) return 1;
    return 0;
}

flux_status_t flux_compose_scan(flux_compose_t* c, const flux_filter_t* f,
                                flux_compose_iter_t** out) {
    if (!c || !out) return FLUX_ERR_ARG;
    flux_compose_iter_t* it = (flux_compose_iter_t*)calloc(1, sizeof(*it));
    if (!it) return FLUX_ERR_NOMEM;
    it->c = c;
    it->has_filter = f ? 1 : 0;
    if (f) it->f = *f;
    it->ids = NULL;
    /* walk layers strongest-first; first sight of an id fixes its resolution */
    for (size_t li = 0; li < c->n_layers; ++li) {
        flux_txn_t* t = NULL;
        if (flux_txn_begin_read(c->layers[li], &t) != FLUX_OK) continue;
        flux_iter_t* lit = NULL;
        flux_scan(t, NULL, &lit);
        flux_record_t r;
        while (flux_iter_next(lit, &r) == FLUX_OK) {
            if (!id_seen(it->ids, it->n_ids, &r.id)) {
                it->ids = realloc(it->ids, (it->n_ids + 1) * sizeof(flux_id_t));
                it->ids[it->n_ids++] = r.id;
            }
            flux_record_free(&r);
        }
        flux_iter_free(lit);
        flux_txn_rollback(t);
    }
    *out = it;
    return FLUX_OK;
}

static int matches(const flux_record_t* r, const flux_filter_t* f) {
    if (f->layer_mask && !(r->layer & f->layer_mask)) return 0;
    if (f->kind && (!r->kind || strcmp(r->kind, f->kind) != 0)) return 0;
    return 1;
}

flux_status_t flux_compose_iter_next(flux_compose_iter_t* it, flux_record_t* out) {
    if (!it || !out) return FLUX_ERR_ARG;
    while (it->cur < it->n_ids) {
        flux_id_t id = it->ids[it->cur++];
        if (flux_compose_get(it->c, &id, out) != FLUX_OK) continue;
        if (it->has_filter && !matches(out, &it->f)) { flux_record_free(out); continue; }
        return FLUX_OK;
    }
    return FLUX_ERR_NOTFOUND;
}

void flux_compose_iter_free(flux_compose_iter_t* it) {
    if (!it) return;
    free(it->ids);
    free(it);
}
