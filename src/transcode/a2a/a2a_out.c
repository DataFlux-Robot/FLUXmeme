/* A2A out: project MIND/kind=agent_card|task records to an A2A-style bundle.
 *   out_dir/
 *     agent-card.json   (kind=agent_card payload, verbatim JSON)
 *     tasks/<id>.json   (kind=task payloads)
 * See SPEC.md §2 (MIND layer). */
#include "fluxmeme/fluxmeme.h"
#include "../fsutil.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

flux_status_t flux_to_a2a(const flux_txn_t* txn, const char* out_dir) {
    if (!txn || !out_dir) return FLUX_ERR_ARG;
    flux_mkdir(out_dir);
    char tasks_dir[1024];
    snprintf(tasks_dir, sizeof(tasks_dir), "%s%ctasks", out_dir, FLUX_PATH_SEP);
    flux_mkdir(tasks_dir);

    flux_iter_t* it = NULL;
    flux_status_t st = flux_scan(txn, NULL, &it); /* all MIND; filter by kind below */
    if (st != FLUX_OK) return st;

    flux_record_t rec;
    int n_card = 0, n_task = 0;
    while (flux_iter_next(it, &rec) == FLUX_OK) {
        if (!rec.kind || (rec.layer & FLUX_LAYER_MIND) == 0) { flux_record_free(&rec); continue; }
        char hex[33];
        flux_id_to_hex(&rec.id, hex);

        if (strcmp(rec.kind, "agent_card") == 0) {
            /* write the first agent_card as agent-card.json */
            char path[1100];
            snprintf(path, sizeof(path), "%s%cagent-card.json", out_dir, FLUX_PATH_SEP);
            FILE* f = fopen(path, "wb");
            if (f) {
                if (rec.payload.len) fwrite(rec.payload.data, 1, rec.payload.len, f);
                fclose(f);
                n_card++;
            }
        } else if (strcmp(rec.kind, "task") == 0 || strcmp(rec.kind, "artifact") == 0) {
            char path[1100];
            const char* sub = (strcmp(rec.kind, "artifact") == 0) ? "artifacts" : "tasks";
            snprintf(path, sizeof(path), "%s%c%s%c%s.json", out_dir, FLUX_PATH_SEP, sub,
                     FLUX_PATH_SEP, hex);
            FILE* f = fopen(path, "wb");
            if (f) {
                if (rec.payload.len) fwrite(rec.payload.data, 1, rec.payload.len, f);
                fclose(f);
                n_task++;
            }
        }
        flux_record_free(&rec);
    }
    flux_iter_free(it);
    (void)n_card; (void)n_task;
    return FLUX_OK;
}
