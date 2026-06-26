/* demo_robot — BODY kinematics facet: closed-loop graph (4-bar) vs tree (chain).
 * Builds records for links + joints, loads the graph, asserts cycle detection —
 * i.e. that closed loops (beyond URDF trees) are first-class. */
#include "fluxmeme/fluxmeme.h"
#include "../src/facets/robot_graph.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHECK(cond, msg)                                                       \
    do { if (!(cond)) { printf("FAIL: %s (%s)\n", msg, flux_last_error()); return 1; } } while (0)

static void add_link(flux_txn_t* w, flux_id_t* id_out, const char* name) {
    flux_meta_kv_t m = { "name", name };
    flux_record_t r;
    memset(&r, 0, sizeof(r));
    r.layer = FLUX_LAYER_BODY;
    r.kind = "robot/link";
    r.meta = &m;
    r.meta_count = 1;
    flux_put(w, &r);
    *id_out = r.id;
}

static void add_joint(flux_txn_t* w, const char* type,
                      const flux_id_t* parent, const flux_id_t* child) {
    static const char* rp = "parent";
    static const char* rc = "child";
    flux_link_t lk[2];
    memset(lk, 0, sizeof(lk));
    memcpy(lk[0].target.bytes, parent->bytes, 16);
    lk[0].rel = rp;
    memcpy(lk[1].target.bytes, child->bytes, 16);
    lk[1].rel = rc;
    flux_meta_kv_t m = { "type", type };
    flux_record_t r;
    memset(&r, 0, sizeof(r));
    r.layer = FLUX_LAYER_BODY;
    r.kind = "robot/joint";
    r.meta = &m;
    r.meta_count = 1;
    r.links = lk;
    r.link_count = 2;
    flux_put(w, &r);
}

int main(void) {
    /* ---- closed loop: 4-bar linkage ---- */
    flux_store_t* sl = NULL;
    CHECK(flux_open("demo_robot_loop.flux", 1, &sl) == FLUX_OK, "open loop store");
    flux_txn_t* wl = NULL;
    CHECK(flux_txn_begin_write(sl, &wl) == FLUX_OK, "begin write loop");
    flux_id_t L[4];
    for (int i = 0; i < 4; ++i) add_link(wl, &L[i], "L");
    add_joint(wl, "revolute", &L[0], &L[1]);
    add_joint(wl, "revolute", &L[1], &L[2]);
    add_joint(wl, "revolute", &L[2], &L[3]);
    add_joint(wl, "revolute", &L[3], &L[0]); /* closes the loop */
    CHECK(flux_txn_commit(wl) == FLUX_OK, "commit loop");

    flux_txn_t* rl = NULL;
    CHECK(flux_txn_begin_read(sl, &rl) == FLUX_OK, "begin read loop");
    flux_robot_graph gl;
    CHECK(flux_robot_load(rl, &gl) == FLUX_OK, "load loop graph");
    CHECK(gl.n_links == 4 && gl.n_joints == 4, "4 links + 4 joints");
    CHECK(flux_robot_has_cycle(&gl) == 1, "4-bar must be a closed loop (not a tree)");
    flux_robot_graph_free(&gl);
    flux_close(sl);

    /* ---- tree: chain ---- */
    flux_store_t* st = NULL;
    CHECK(flux_open("demo_robot_tree.flux", 1, &st) == FLUX_OK, "open tree store");
    flux_txn_t* wt = NULL;
    CHECK(flux_txn_begin_write(st, &wt) == FLUX_OK, "begin write tree");
    flux_id_t C[3];
    for (int i = 0; i < 3; ++i) add_link(wt, &C[i], "C");
    add_joint(wt, "revolute", &C[0], &C[1]);
    add_joint(wt, "revolute", &C[1], &C[2]);
    CHECK(flux_txn_commit(wt) == FLUX_OK, "commit tree");

    flux_txn_t* rt = NULL;
    CHECK(flux_txn_begin_read(st, &rt) == FLUX_OK, "begin read tree");
    flux_robot_graph gt;
    CHECK(flux_robot_load(rt, &gt) == FLUX_OK, "load tree graph");
    CHECK(gt.n_links == 3 && gt.n_joints == 2, "3 links + 2 joints");
    CHECK(flux_robot_has_cycle(&gt) == 0, "chain must be a tree (no cycle)");
    flux_robot_graph_free(&gt);
    flux_close(st);

    printf("PASS: demo_robot closed-loop(4-bar=cycle) + tree(chain=no-cycle) OK\n");
    return 0;
}
