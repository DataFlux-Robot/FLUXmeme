/* device-comm — the BODY device/bus topology facet.
 *
 * Models the wiring a robot's transport codecs route over: devices + buses as
 * nodes, connections as edges carrying a protocol. Records:
 *   kind=device-comm/node  device or bus (meta: name, kind=device|bus,
 *                           protocol=CAN|RS485|EtherCAT|MAVLink|..., baud, addr)
 *   kind=device-comm/edge  connection, links: {a,"a"},{b,"b"}; meta: protocol
 * The topology is the addressing/routing basis for the MAVLink/CAN/EtherCAT
 * frame codecs. See SPEC §6C. */
#ifndef FLUX_DEVICE_COMM_H
#define FLUX_DEVICE_COMM_H
#include "fluxmeme/fluxmeme.h"

typedef struct {
    flux_id_t id;
    char name[64];
    char kind[8];      /* "device" | "bus" */
    char protocol[16]; /* CAN | RS485 | EtherCAT | MAVLink | ... */
    char addr[32];     /* node address / id on the bus */
    uint64_t baud;     /* 0 for non-serial (e.g. EtherCAT) */
} flux_dnode;

typedef struct {
    flux_id_t id;
    flux_id_t a;
    flux_id_t b;
    char protocol[16];
} flux_dedge;

typedef struct {
    flux_dnode* nodes;
    size_t n_nodes;
    flux_dedge* edges;
    size_t n_edges;
} flux_dcomm;

flux_status_t flux_dcomm_load(const flux_txn_t* txn, flux_dcomm* out);
void flux_dcomm_free(flux_dcomm* g);

/* Count + collect edges using `protocol` (e.g. "RS485"). *out is a borrowed
 * pointer into a static/thread-local buffer of size count (valid until next
 * call). The codec uses this to know which links a protocol runs over. */
size_t flux_dcomm_edges_by_protocol(const flux_dcomm* g, const char* protocol,
                                    const flux_dedge** out);

#endif
