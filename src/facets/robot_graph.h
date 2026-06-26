/* robot-graph — the BODY kinematics facet.
 *
 * A robot is modelled as a GENERAL graph (links + joints + constraints), not a
 * URDF tree. Closed loops (4-bar, Delta, Stewart) are first-class. Records:
 *   kind=robot/link     rigid body (meta: name)
 *   kind=robot/joint    edge, links: {parent_link,"parent"},{child_link,"child"};
 *                       meta: type (revolute|prismatic|fixed|continuous|...)
 *   kind=robot/constraint closed-loop coupling, links: {a,"a"},{b,"b"}
 * See SPEC §1.2 / §6C. */
#ifndef FLUX_ROBOT_GRAPH_H
#define FLUX_ROBOT_GRAPH_H
#include "fluxmeme/fluxmeme.h"

typedef struct {
    flux_id_t id;
    char name[64];
} flux_rlink;

typedef struct {
    flux_id_t id;
    flux_id_t parent;
    flux_id_t child;
    char type[16]; /* revolute|prismatic|fixed|continuous|planar|floating */
} flux_rjoint;

typedef struct {
    flux_rlink* links;
    size_t n_links;
    flux_rjoint* joints;
    size_t n_joints;
} flux_robot_graph;

/* load the graph from a txn snapshot (BODY kind=robot/* records). */
flux_status_t flux_robot_load(const flux_txn_t* txn, flux_robot_graph* out);
void flux_robot_graph_free(flux_robot_graph* g);

/* 1 if the kinematic graph has a closed loop (i.e. is NOT a pure tree) — the
 * capability URDF cannot express. */
int flux_robot_has_cycle(const flux_robot_graph* g);

#endif
