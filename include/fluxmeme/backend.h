/* FLUXmeme — storage backend abstraction.
 *
 * The on-disk format is identical everywhere (Tier 1). Backends (Tier 2)
 * absorb platform differences (mmap / flash / file). v1 ships flux_backend_file;
 * mmap and flash (FlashDB-backed) are designed-for and added per-platform. */
#ifndef FLUXMEME_BACKEND_H
#define FLUXMEME_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct flux_backend {
    /* open(self, path, writable) -> status; may be NULL if already bound. */
    flux_status_t (*open)(void* self, const char* path, int writable);
    /* read(self, off, buf, n) -> bytes read (<0 on error). */
    int64_t (*read)(void* self, uint64_t off, void* buf, size_t n);
    /* append(self, buf, n) -> bytes written (<0 on error); grows the file. */
    int64_t (*append)(void* self, const void* buf, size_t n);
    /* fsync the latest appends. */
    flux_status_t (*sync)(void* self);
    /* exclusive (write) lock; blocking. */
    flux_status_t (*lock)(void* self, int exclusive);
    flux_status_t (*unlock)(void* self);
    /* current file size in bytes. */
    flux_status_t (*size)(void* self, uint64_t* out);
    flux_status_t (*close)(void* self);
    void* self;
} flux_backend_t;

/* The shipped portable file backend (stdio + pread/pwrite + flock/LockFileEx). */
flux_backend_t* flux_backend_file(void); /* singleton; do not free. */

#ifdef __cplusplus
}
#endif
#endif /* FLUXMEME_BACKEND_H */
