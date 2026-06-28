/* demo_device — device-comm facet: device/bus topology + protocol routing.
 * Builds an RS485 bus with 2 motors + a MAVLink radio, loads the topology,
 * asserts structure and the per-protocol edge query the codec routes over. */
#include "fluxmeme/fluxmeme.h"
#include "../src/core/ref_utils.h"
#include "../src/facets/device_comm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHECK(cond, msg)                                                       \
    do { if (!(cond)) { printf("FAIL: %s (%s)\n", msg, flux_last_error()); return 1; } } while (0)

static void add_node(flux_txn_t* w, flux_id_t* id_out, const char* name,
                     const char* kind, const char* protocol, const char* addr,
                     uint64_t baud) {
    char baudbuf[24];
    snprintf(baudbuf, sizeof baudbuf, "%llu", (unsigned long long)baud);
    flux_meta_kv_t m[5];
    m[0].key = "name";     m[0].val = name;       m[0].type = FLUX_META_STRING;
    m[1].key = "kind";     m[1].val = kind;       m[1].type = FLUX_META_STRING;
    m[2].key = "protocol"; m[2].val = protocol;   m[2].type = FLUX_META_STRING;
    m[3].key = "addr";     m[3].val = addr;       m[3].type = FLUX_META_STRING;
    m[4].key = "baud";     m[4].val = baudbuf;    m[4].type = FLUX_META_STRING;
    flux_record_t r;
    memset(&r, 0, sizeof(r));
    r.layer = FLUX_LAYER_BODY;
    r.kind = "device-comm/node";
    r.meta = m;
    r.meta_count = 5;
    flux_put(w, &r);
    *id_out = r.id;
}

static void add_edge(flux_txn_t* w, const char* protocol,
                     const flux_id_t* a, const flux_id_t* b) {
    /* endpoints a/b are REF-typed meta entries (keys "a"/"b") */
    static char ahex[33], bhex[33];
    static char aref[80], bref[80];
    static flux_meta_kv_t em[3];
    flux_id_to_hex(a, ahex);
    flux_id_to_hex(b, bhex);
    flux_ref_encode(aref, sizeof(aref), ahex, NULL);
    flux_ref_encode(bref, sizeof(bref), bhex, NULL);
    em[0].key = "protocol"; em[0].val = protocol; em[0].type = FLUX_META_STRING;
    em[1].key = "a";        em[1].val = aref;     em[1].type = FLUX_META_REF;
    em[2].key = "b";        em[2].val = bref;     em[2].type = FLUX_META_REF;
    flux_record_t r;
    memset(&r, 0, sizeof(r));
    r.layer = FLUX_LAYER_BODY;
    r.kind = "device-comm/edge";
    r.meta = em;
    r.meta_count = 3;
    flux_put(w, &r);
}

int main(void) {
    flux_store_t* s = NULL;
    CHECK(flux_open("demo_device.flux", 1, &s) == FLUX_OK, "open");
    flux_txn_t* w = NULL;
    CHECK(flux_txn_begin_write(s, &w) == FLUX_OK, "begin write");

    flux_id_t bus, ml, mr, radio;
    add_node(w, &bus,   "rs485_bus", "bus",    "RS485",   "0",  115200);
    add_node(w, &ml,    "motor_l",   "device", "RS485",   "1",  0);
    add_node(w, &mr,    "motor_r",   "device", "RS485",   "2",  0);
    add_node(w, &radio, "mav_radio", "device", "MAVLink", "255", 0);
    add_edge(w, "RS485",   &ml,  &bus);
    add_edge(w, "RS485",   &mr,  &bus);
    CHECK(flux_txn_commit(w) == FLUX_OK, "commit");

    flux_txn_t* r1 = NULL;
    CHECK(flux_txn_begin_read(s, &r1) == FLUX_OK, "begin read");
    flux_dcomm dc;
    CHECK(flux_dcomm_load(r1, &dc) == FLUX_OK, "load topology");
    CHECK(dc.n_nodes == 4, "4 nodes");
    CHECK(dc.n_edges == 2, "2 edges");

    /* count devices vs buses */
    int ndev = 0, nbus = 0;
    for (size_t i = 0; i < dc.n_nodes; ++i) {
        if (strcmp(dc.nodes[i].kind, "device") == 0) ndev++;
        else if (strcmp(dc.nodes[i].kind, "bus") == 0) nbus++;
    }
    CHECK(ndev == 3 && nbus == 1, "3 devices + 1 bus");

    /* protocol routing: the RS485 codec should run over exactly the 2 RS485 edges */
    const flux_dedge* rs485 = NULL;
    size_t n = flux_dcomm_edges_by_protocol(&dc, "RS485", &rs485);
    CHECK(n == 2, "2 RS485 edges for the codec to route over");
    CHECK(strcmp(dc.nodes[0].protocol, "RS485") == 0 && dc.nodes[0].baud == 115200,
          "bus protocol+baud preserved");

    flux_dcomm_free(&dc);
    flux_close(s);
    printf("PASS: demo_device topology (4 nodes/2 edges, RS485 routing query) OK\n");
    return 0;
}
