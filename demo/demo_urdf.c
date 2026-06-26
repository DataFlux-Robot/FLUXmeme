/* demo_urdf — URDF/SDF XML -> BODY robot-graph records.
 * Writes a 2-link/1-joint URDF, imports, loads the graph, asserts structure,
 * the parent/child wiring, and that URDF's tree nature is preserved (no cycle).
 * Then repeats with an SDF sample (same <link>/<joint> shape). */
#include "fluxmeme/fluxmeme.h"
#include "../src/facets/robot_graph.h"
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

static const char* URDF =
    "<robot name=\"arm\">\n"
    "  <link name=\"base\"/>\n"
    "  <link name=\"link1\"/>\n"
    "  <joint name=\"base_link1\" type=\"revolute\">\n"
    "    <parent link=\"base\"/>\n"
    "    <child link=\"link1\"/>\n"
    "    <axis xyz=\"0 0 1\"/>\n"
    "    <limit lower=\"-1\" upper=\"1\"/>\n"
    "  </joint>\n"
    "</robot>\n";

static const char* SDF =
    "<sdf version=\"1.6\">\n"
    "  <model name=\"m\">\n"
    "    <link name=\"b\"/>\n"
    "    <link name=\"l\"/>\n"
    "    <joint name=\"j\" type=\"fixed\">\n"
    "      <parent link=\"b\"/>\n"
    "      <child link=\"l\"/>\n"
    "    </joint>\n"
    "  </model>\n"
    "</sdf>\n";

static void write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "wb");
    if (f) { fputs(content, f); fclose(f); }
}

static const char* link_name(const flux_robot_graph* g, const flux_id_t* id) {
    for (size_t i = 0; i < g->n_links; ++i)
        if (memcmp(g->links[i].id.bytes, id->bytes, 16) == 0) return g->links[i].name;
    return "?";
}

int main(void) {
    write_file("sample.urdf", URDF);

    /* URDF import */
    flux_store_t* s = NULL;
    CHECK(flux_open("demo_urdf.flux", 1, &s) == FLUX_OK, "open");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(s, &w) == FLUX_OK, "begin write");
    CHECK(flux_from_urdf("sample.urdf", w) == FLUX_OK, "from_urdf");
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit");

    flux_txn_t* r1 = NULL;
    CHECK(flux_txn_begin_read(s, &r1) == FLUX_OK, "begin read");
    flux_robot_graph g;
    CHECK(flux_robot_load(r1, &g) == FLUX_OK, "load graph");
    CHECK(g.n_links == 2, "2 links from URDF");
    CHECK(g.n_joints == 1, "1 joint from URDF");
    CHECK(flux_robot_has_cycle(&g) == 0, "URDF is a tree (no cycle)");
    CHECK(g.n_joints == 1 && strcmp(link_name(&g, &g.joints[0].parent), "base") == 0 &&
              strcmp(link_name(&g, &g.joints[0].child), "link1") == 0,
          "joint wires base->link1");
    CHECK(strcmp(g.joints[0].type, "revolute") == 0, "joint type preserved");
    flux_robot_graph_free(&g);
    flux_close(s);

    /* SDF import */
    write_file("sample.sdf", SDF);
    flux_store_t* s2 = NULL;
    CHECK(flux_open("demo_sdf.flux", 1, &s2) == FLUX_OK, "open sdf");
    flux_txn_t* w2 = NULL;
    CHECK(flux_txn_begin_write(s2, &w2) == FLUX_OK, "begin write sdf");
    CHECK(flux_from_sdf("sample.sdf", w2) == FLUX_OK, "from_sdf");
    CHECK(flux_txn_commit(w2) == FLUX_OK, "commit sdf");
    flux_txn_t* r2 = NULL;
    CHECK(flux_txn_begin_read(s2, &r2) == FLUX_OK, "begin read sdf");
    flux_robot_graph g2;
    CHECK(flux_robot_load(r2, &g2) == FLUX_OK, "load sdf graph");
    CHECK(g2.n_links == 2 && g2.n_joints == 1, "SDF: 2 links + 1 joint");
    CHECK(strcmp(g2.joints[0].type, "fixed") == 0, "SDF joint type fixed");
    flux_robot_graph_free(&g2);
    flux_close(s2);

    printf("PASS: demo_urdf URDF(2 links/1 revolute joint/tree) + SDF(2/1 fixed) import OK\n");
    return 0;
}
