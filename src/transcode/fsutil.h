/* Tiny cross-platform fs helpers for transcoders. */
#ifndef FLUX_FSUTIL_H
#define FLUX_FSUTIL_H

#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define FLUX_PATH_SEP '\\'
static int flux_mkdir(const char* path) { return _mkdir(path); }
#else
#include <sys/stat.h>
#include <dirent.h>
#define FLUX_PATH_SEP '/'
static int flux_mkdir(const char* path) { return mkdir(path, 0755); }
#endif

typedef void (*flux_dir_cb)(const char* name, void* ud);

/* List entries in `path` (excluding "." and ".."), invoking cb for each.
 * Returns entry count, or -1 on error. */
static int flux_list_dir(const char* path, flux_dir_cb cb, void* ud) {
    int count = 0;
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    struct _finddata_t fd;
    intptr_t h = _findfirst(pattern, &fd);
    if (h == -1) return -1;
    do {
        if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
        cb(fd.name, ud);
        count++;
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
#else
    DIR* d = opendir(path);
    if (!d) return -1;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        cb(e->d_name, ud);
        count++;
    }
    closedir(d);
#endif
    return count;
}

#endif /* FLUX_FSUTIL_H */
