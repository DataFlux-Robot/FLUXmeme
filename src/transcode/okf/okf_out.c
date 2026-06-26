/* OKF out: project MIND/kind=concept records to a markdown bundle.
 *   out_dir/
 *     concepts/<id>.md   (frontmatter from meta + markdown body)
 *     index.md           (titles + links, by ts)
 *     log.md             (append-order activity)
 * See SPEC.md §2 (MIND layer). */
#include "fluxmeme/fluxmeme.h"
#include "../fsutil.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int has_suffix(const char* s, const char* suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

static void join_path(char* out, size_t out_sz, const char* a, const char* b) {
    snprintf(out, out_sz, "%s%c%s", a, FLUX_PATH_SEP, b);
}

flux_status_t flux_to_okf(const flux_txn_t* txn, const char* out_dir) {
    if (!txn || !out_dir) return FLUX_ERR_ARG;
    flux_mkdir(out_dir);
    char concepts_dir[1024];
    snprintf(concepts_dir, sizeof(concepts_dir), "%s%cconcepts", out_dir, FLUX_PATH_SEP);
    flux_mkdir(concepts_dir);

    char index_path[1024], log_path[1024];
    join_path(index_path, sizeof(index_path), out_dir, "index.md");
    join_path(log_path, sizeof(log_path), out_dir, "log.md");
    FILE* fidx = fopen(index_path, "wb");
    FILE* flog = fopen(log_path, "wb");
    if (!fidx || !flog) { if (fidx) fclose(fidx); if (flog) fclose(flog); return FLUX_ERR_IO; }
    fprintf(fidx, "# Index\n\n");
    fprintf(flog, "# Log\n\n");

    flux_filter_t f;
    memset(&f, 0, sizeof(f));
    f.layer_mask = FLUX_LAYER_MIND;
    f.kind = "concept";

    flux_iter_t* it = NULL;
    flux_status_t st = flux_scan(txn, &f, &it);
    if (st != FLUX_OK) { fclose(fidx); fclose(flog); return st; }

    flux_record_t rec;
    int count = 0;
    while (flux_iter_next(it, &rec) == FLUX_OK) {
        char hex[33];
        flux_id_to_hex(&rec.id, hex);
        char md_path[1100];
        snprintf(md_path, sizeof(md_path), "%s%c%s.md", concepts_dir, FLUX_PATH_SEP, hex);
        FILE* fm = fopen(md_path, "wb");
        if (!fm) { flux_record_free(&rec); continue; }

        /* frontmatter (meta carries type/title/tags/...) */
        fprintf(fm, "---\n");
        const char* title = NULL;
        for (uint32_t i = 0; i < rec.meta_count; ++i) {
            fprintf(fm, "%s: %s\n", rec.meta[i].key, rec.meta[i].val);
            if (strcmp(rec.meta[i].key, "title") == 0) title = rec.meta[i].val;
        }
        if (!title) title = hex;
        fprintf(fm, "timestamp: %llu\n", (unsigned long long)rec.ts);
        fprintf(fm, "---\n\n");

        /* body (assume markdown) */
        if (rec.payload.len)
            fwrite(rec.payload.data, 1, rec.payload.len, fm);

        /* links as wikilinks */
        if (rec.link_count) {
            fprintf(fm, "\n## Links\n");
            for (uint32_t i = 0; i < rec.link_count; ++i) {
                char th[33];
                flux_id_to_hex(&rec.links[i].target, th);
                const char* rel = rec.links[i].rel ? rec.links[i].rel : "related";
                fprintf(fm, "- [[%s]] (%s)\n", th, rel);
            }
        }
        fclose(fm);

        fprintf(fidx, "- [%s](concepts/%s.md)\n", title, hex);
        fprintf(flog, "- %s ts=%llu %s\n", hex, (unsigned long long)rec.ts, title);
        count++;
        flux_record_free(&rec);
    }
    flux_iter_free(it);
    fclose(fidx);
    fclose(flog);
    return count > 0 ? FLUX_OK : FLUX_OK;
}
