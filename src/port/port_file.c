/* Portable file backend: stdio/Win32 file I/O + cross-platform exclusive lock.
 * Shipped default (Tier 2). mmap and flash backends are designed-for and added
 * per-platform; the on-disk format is identical across all of them. */
#include "fluxmeme/types.h"
#include "fluxmeme/backend.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HANDLE port_fh;
#define PORT_BAD INVALID_HANDLE_VALUE
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
typedef int port_fh;
#define PORT_BAD (-1)
#endif

typedef struct {
    port_fh fh;
    int writable;
    uint64_t size;
} port_state_t;

/* ---- open ---- */
static flux_status_t port_open(void* selfv, const char* path, int writable) {
    port_state_t* st = (port_state_t*)selfv;
#ifdef _WIN32
    DWORD access = GENERIC_READ | (writable ? GENERIC_WRITE : 0);
    DWORD share = FILE_SHARE_READ;
    DWORD disp = writable ? OPEN_ALWAYS : OPEN_ALWAYS; /* read-only open still OPEN_ALWAYS */
    HANDLE h = CreateFileA(path, access, share, NULL, disp, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FLUX_ERR_IO;
    st->fh = h;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz)) { CloseHandle(h); return FLUX_ERR_IO; }
    st->size = (uint64_t)sz.QuadPart;
#else
    int flags = writable ? (O_RDWR | O_CREAT) : O_RDONLY;
    int fd = open(path, flags, 0644);
    if (fd < 0) return FLUX_ERR_IO;
    st->fh = fd;
    struct stat sb;
    if (fstat(fd, &sb) == 0) st->size = (uint64_t)sb.st_size;
    else st->size = 0;
#endif
    st->writable = writable;
    return FLUX_OK;
}

/* ---- read ---- */
static int64_t port_read(void* selfv, uint64_t off, void* buf, size_t n) {
    port_state_t* st = (port_state_t*)selfv;
#ifdef _WIN32
    OVERLAPPED ov; memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)(off & 0xffffffff);
    ov.OffsetHigh = (DWORD)(off >> 32);
    DWORD got = 0;
    if (!ReadFile(st->fh, buf, (DWORD)n, &got, &ov)) return -1;
    return (int64_t)got;
#else
    ssize_t got = pread(st->fh, buf, n, (off_t)off);
    return got;
#endif
}

/* ---- append (write at end) ---- */
static int64_t port_append(void* selfv, const void* buf, size_t n) {
    port_state_t* st = (port_state_t*)selfv;
#ifdef _WIN32
    LARGE_INTEGER pos;
    pos.QuadPart = 0;
    if (!SetFilePointerEx(st->fh, pos, &pos, FILE_END)) return -1;
    OVERLAPPED ov; memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)(pos.QuadPart & 0xffffffff);
    ov.OffsetHigh = (DWORD)(pos.QuadPart >> 32);
    DWORD wrote = 0;
    if (!WriteFile(st->fh, buf, (DWORD)n, &wrote, &ov)) return -1;
    st->size += wrote;
    return (int64_t)wrote;
#else
    off_t pos = lseek(st->fh, 0, SEEK_END);
    if (pos < 0) return -1;
    ssize_t wrote = write(st->fh, buf, n);
    if (wrote < 0) return -1;
    st->size += (uint64_t)wrote;
    return wrote;
#endif
}

static flux_status_t port_sync(void* selfv) {
    port_state_t* st = (port_state_t*)selfv;
#ifdef _WIN32
    return FlushFileBuffers(st->fh) ? FLUX_OK : FLUX_ERR_IO;
#else
    return (fsync(st->fh) == 0) ? FLUX_OK : FLUX_ERR_IO;
#endif
}

static flux_status_t port_lock(void* selfv, int exclusive) {
    port_state_t* st = (port_state_t*)selfv;
    (void)exclusive;
#ifdef _WIN32
    OVERLAPPED ov; memset(&ov, 0, sizeof(ov));
    return LockFileEx(st->fh, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &ov)
               ? FLUX_OK
               : FLUX_ERR_LOCKED;
#else
    return (flock(st->fh, LOCK_EX | LOCK_NB) == 0) ? FLUX_OK : FLUX_ERR_LOCKED;
#endif
}

static flux_status_t port_unlock(void* selfv) {
    port_state_t* st = (port_state_t*)selfv;
#ifdef _WIN32
    OVERLAPPED ov; memset(&ov, 0, sizeof(ov));
    UnlockFileEx(st->fh, 0, MAXDWORD, MAXDWORD, &ov);
    return FLUX_OK;
#else
    flock(st->fh, LOCK_UN);
    return FLUX_OK;
#endif
}

static flux_status_t port_size(void* selfv, uint64_t* out) {
    port_state_t* st = (port_state_t*)selfv;
    *out = st->size;
    return FLUX_OK;
}

static flux_status_t port_close(void* selfv) {
    port_state_t* st = (port_state_t*)selfv;
#ifdef _WIN32
    if (st->fh != INVALID_HANDLE_VALUE) CloseHandle(st->fh);
#else
    if (st->fh >= 0) close(st->fh);
#endif
    free(st);
    return FLUX_OK;
}

flux_backend_t* flux_backend_file(void) {
    port_state_t* st = (port_state_t*)calloc(1, sizeof(port_state_t));
    if (!st) return NULL;
    flux_backend_t* be = (flux_backend_t*)calloc(1, sizeof(flux_backend_t));
    if (!be) { free(st); return NULL; }
    be->self = st;
    be->open = port_open;
    be->read = port_read;
    be->append = port_append;
    be->sync = port_sync;
    be->lock = port_lock;
    be->unlock = port_unlock;
    be->size = port_size;
    be->close = port_close;
    return be;
}
