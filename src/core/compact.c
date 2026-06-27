/* flux_compact — reclaim append-only growth by rewriting a store with only its
 * live records (drops tombstones, superseded versions, and torn tail). The
 * rewrite goes to a temp file, then replaces the original (remove + rename:
 * Windows' rename does not reliably replace an existing target). */
#include "fluxmeme/fluxmeme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

flux_status_t flux_compact(const char* path) {
    if (!path) return FLUX_ERR_ARG;

    flux_store_t* src = NULL;
    if (flux_open(path, 0, &src) != FLUX_OK)
        return FLUX_ERR_IO;

    char tmp[1100];
    int wn = snprintf(tmp, sizeof(tmp), "%s.compact.tmp", path);
    if (wn < 0 || (size_t)wn >= sizeof(tmp)) { flux_close(src); return FLUX_ERR_ARG; }

    flux_store_t* dst = NULL;
    if (flux_open(tmp, 1, &dst) != FLUX_OK) { flux_close(src); return FLUX_ERR_IO; }

    flux_txn_t* rt = NULL;
    flux_txn_t* wt = NULL;
    if (flux_txn_begin_read(src, &rt) != FLUX_OK || flux_txn_begin_write(dst, &wt) != FLUX_OK) {
        if (rt) flux_txn_rollback(rt);
        flux_close(dst);
        flux_close(src);
        remove(tmp);
        return FLUX_ERR_IO;
    }

    flux_iter_t* it = NULL;
    if (flux_scan(rt, NULL, &it) == FLUX_OK) {
        flux_record_t r;
        while (flux_iter_next(it, &r) == FLUX_OK) {
            flux_put(wt, &r); /* keep the id (composition/refs hold); ver -> dst seq */
            flux_record_free(&r);
        }
        flux_iter_free(it);
    }
    flux_txn_commit(wt);
    flux_txn_rollback(rt);
    flux_close(dst);
    flux_close(src);

    /* atomic-ish replace: MSVC rename won't replace an existing target. */
    if (remove(path) != 0) { remove(tmp); return FLUX_ERR_IO; }
    if (rename(tmp, path) != 0) { remove(tmp); return FLUX_ERR_IO; }
    return FLUX_OK;
}
