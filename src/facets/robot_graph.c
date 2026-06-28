/* robot-graph — BODY kinematics facet: load + closed-loop (cycle) detection. */
#include "robot_graph.h"
#include "../core/ref_utils.h"
#include <stdlib.h>
#include <string.h>

static const char* meta_val(const flux_record_t* r, const char* key) {
    for (uint32_t i = 0; i < r->meta_count; ++i)
        if (r->meta[i].key && strcmp(r->meta[i].key, key) == 0)
            return r->meta[i].val ? r->meta[i].val : "";
    return "";
}

static void copy_str(char* dst, size_t n, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

flux_status_t flux_robot_load(const flux_txn_t* txn, flux_robot_graph* out) {
    if (!txn || !out) return FLUX_ERR_ARG;
    memset(out, 0, sizeof(*out));
    flux_iter_t* it = NULL;
    flux_status_t st = flux_scan(txn, NULL, &it);
    if (st != FLUX_OK) return st;
    flux_record_t r;
    while (flux_iter_next(it, &r) == FLUX_OK) {
        if (!(r.layer & FLUX_LAYER_BODY) || !r.kind) { flux_record_free(&r); continue; }
        if (strcmp(r.kind, "robot/link") == 0) {
            out->links = realloc(out->links, (out->n_links + 1) * sizeof(flux_rlink));
            flux_rlink* L = &out->links[out->n_links++];
            memset(L, 0, sizeof(*L));
            L->id = r.id;
            copy_str(L->name, sizeof(L->name), meta_val(&r, "name"));
        } else if (strcmp(r.kind, "robot/joint") == 0) {
            out->joints = realloc(out->joints, (out->n_joints + 1) * sizeof(flux_rjoint));
            flux_rjoint* J = &out->joints[out->n_joints++];
            memset(J, 0, sizeof(*J));
            J->id = r.id;
            copy_str(J->type, sizeof(J->type), meta_val(&r, "type"));
            /* parent/child are REF-typed meta entries (keys "parent"/"child") */
            char hex33[33]; const char* graph = NULL;
            if (flux_ref_by_rel(&r, "parent", hex33, &graph) == FLUX_OK)
                flux_id_from_hex(hex33, &J->parent);
            if (flux_ref_by_rel(&r, "child", hex33, &graph) == FLUX_OK)
                flux_id_from_hex(hex33, &J->child);
        }
        flux_record_free(&r);
    }
    flux_iter_free(it);
    return FLUX_OK;
}

void flux_robot_graph_free(flux_robot_graph* g) {
    if (!g) return;
    free(g->links);
    free(g->joints);
    memset(g, 0, sizeof(*g));
}

/* find link index by id */
static int link_index(const flux_robot_graph* g, const flux_id_t* id) {
    for (size_t i = 0; i < g->n_links; ++i)
        if (memcmp(g->links[i].id.bytes, id->bytes, 16) == 0) return (int)i;
    return -1;
}

/* DFS for cycle in the undirected link graph formed by joints. */
static int dfs_cycle(const flux_robot_graph* g, int u, int parent,
                     char* visited, int* parent_of) {
    visited[u] = 1;
    /* for each neighbor v (via any joint connecting u) */
    for (size_t j = 0; j < g->n_joints; ++j) {
        int a = link_index(g, &g->joints[j].parent);
        int b = link_index(g, &g->joints[j].child);
        int v = -1;
        if (a == u) v = b;
        else if (b == u) v = a;
        if (v < 0) continue;
        if (!visited[v]) {
            parent_of[v] = u;
            if (dfs_cycle(g, v, u, visited, parent_of)) return 1;
        } else if (v != parent) {
            return 1; /* back edge -> cycle (closed loop) */
        }
    }
    return 0;
}

int flux_robot_has_cycle(const flux_robot_graph* g) {
    if (!g || g->n_links == 0) return 0;
    char* visited = (char*)calloc(g->n_links, 1);
    int* parent_of = (int*)malloc(g->n_links * sizeof(int));
    for (size_t i = 0; i < g->n_links; ++i) parent_of[i] = -1;
    int found = 0;
    for (size_t i = 0; i < g->n_links && !found; ++i) {
        if (!visited[i])
            found = dfs_cycle(g, (int)i, -1, visited, parent_of);
    }
    free(visited);
    free(parent_of);
    return found;
}
