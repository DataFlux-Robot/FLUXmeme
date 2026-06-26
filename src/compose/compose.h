/* composition — LIVRPS strength-ordered, field-level merge over a layer stack.
 *
 * The root .flux carries a record kind="flux/compose" with meta "sublayers" =
 * a ';'-separated list of layer paths (relative to root), strongest first.
 * Resolution per id (LIVRPS: Local > Inherits > Variants > References >
 * Payloads > Specializes): collect the active live versions across the stack
 * (strongest first), then MERGE FIELD BY FIELD — the strongest layer that
 * provides a field wins; weaker layers fill gaps. Non-destructive.
 *
 * Variants: a version tagged meta flux_variant_set=S, flux_variant=V is active
 * only when that set's selected value is V (see flux_compose_set_variant);
 * un-tagged versions are always active (the base). See SPEC §1.4 / §6A.
 */
#ifndef FLUX_COMPOSE_H
#define FLUX_COMPOSE_H
#include "fluxmeme/fluxmeme.h"

typedef struct flux_compose flux_compose_t;

flux_status_t flux_compose_open(const char* root_path, flux_compose_t** out);
void flux_compose_close(flux_compose_t* c);
size_t flux_compose_n_layers(const flux_compose_t* c);

/* Select the active variant value for a variant set. Records tagged with that
 * set+value become active (and stronger than the base); other tagged records
 * are hidden. value="" clears the selection. */
flux_status_t flux_compose_set_variant(flux_compose_t* c, const char* set, const char* value);

/* Resolve one id: field-level merge of its active versions across the stack. */
flux_status_t flux_compose_get(flux_compose_t* c, const flux_id_t* id, flux_record_t* out);

typedef struct flux_compose_iter flux_compose_iter_t;
flux_status_t flux_compose_scan(flux_compose_t* c, const flux_filter_t* f,
                                flux_compose_iter_t** out);
flux_status_t flux_compose_iter_next(flux_compose_iter_t* it, flux_record_t* out);
void flux_compose_iter_free(flux_compose_iter_t* it);

#endif
