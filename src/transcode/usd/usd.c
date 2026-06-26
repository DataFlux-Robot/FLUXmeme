/* USD transcoder — BODY <-> USDA.
 * v1.0 increment; minimal emit (mesh/xform) + minimal recursive-descent parse.
 * This file currently holds the symbol so the lib links; full impl lands in the
 * dedicated USD increment (see roadmap). */
#include "fluxmeme/fluxmeme.h"

flux_status_t flux_to_usd(const flux_txn_t* txn, const char* out_usda) {
    (void)txn;
    (void)out_usda;
    flux_set_error("USD transcoder: not yet implemented (v1.0 increment)");
    return FLUX_ERR_ARG;
}

flux_status_t flux_from_usd(const char* in_usda, flux_txn_t* txn) {
    (void)in_usda;
    (void)txn;
    flux_set_error("USD transcoder: not yet implemented (v1.0 increment)");
    return FLUX_ERR_ARG;
}
