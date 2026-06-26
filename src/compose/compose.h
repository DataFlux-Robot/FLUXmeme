/* composition — minimal LIVRPS subset: a read-time merged view over a layer
 * stack (root + sublayers), id-keyed record-level override, non-destructive.
 *
 * The root .flux carries a record kind="flux/compose" with meta "sublayers" =
 * a ';'-separated list of layer paths (relative to the root), ordered strongest
 * first. Resolution: for a given id, the strongest layer (root, then sublayers
 * in order) that holds a LIVE version wins; weaker layers fill ids the stronger
 * lack. See SPEC §1.4 / §6A.
 *
 * (Full LIVRPS strength arcs — variant/inherit/specialize + field-level merge —
 * are the v1.2 increment; this is the record-level subset.) */
#ifndef FLUX_COMPOSE_H
#define FLUX_COMPOSE_H
#include "fluxmeme/fluxmeme.h"

typedef struct flux_compose flux_compose_t;

flux_status_t flux_compose_open(const char* root_path, flux_compose_t** out);
void flux_compose_close(flux_compose_t* c);
size_t flux_compose_n_layers(const flux_compose_t* c);

/* Resolve one id across the layer stack (strongest live version). */
flux_status_t flux_compose_get(flux_compose_t* c, const flux_id_t* id, flux_record_t* out);

/* Iterate the merged view: each unique id once, resolved to its strongest live
 * version. filter may be NULL. */
typedef struct flux_compose_iter flux_compose_iter_t;
flux_status_t flux_compose_scan(flux_compose_t* c, const flux_filter_t* f,
                                flux_compose_iter_t** out);
flux_status_t flux_compose_iter_next(flux_compose_iter_t* it, flux_record_t* out);
void flux_compose_iter_free(flux_compose_iter_t* it);

#endif
