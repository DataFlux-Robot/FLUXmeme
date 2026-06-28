/* flux — CLI for the self-describing FLUXmeme format.
 *   flux inspect <file>                      identify + stats
 *   flux dump <file>                         list records (id/layer/kind/path)
 *   flux transcode <file> <okf|a2a|usd> <out>  project the composed view
 *   flux compose <root> [--variant S=V]...   resolve the LIVRPS merged view */
#include "fluxmeme/fluxmeme.h"
#include "../src/compose/compose.h"
#include <stdio.h>
#include <string.h>

static const char* layer_name(int layer) {
    if (layer == FLUX_LAYER_BODY) return "BODY";
    if (layer == FLUX_LAYER_MIND) return "MIND";
    if (layer == FLUX_LAYER_JOURNAL) return "JOURNAL";
    return "?";
}

static void payload_preview(const flux_record_t* r) {
    size_t n = r->payload.len < 40 ? r->payload.len : 40;
    fputs("    \"", stdout);
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = r->payload.data[i];
        if (c >= 32 && c < 127) putchar(c);
        else printf("\\x%02x", c);
    }
    if (r->payload.len > 40) fputs("...", stdout);
    fputs("\"\n", stdout);
}

static int cmd_inspect(const char* path) {
    flux_store_t* s = NULL;
    if (flux_open(path, 0, &s) != FLUX_OK) {
        printf("error: %s\n", flux_last_error());
        return 1;
    }
    flux_txn_t* t = NULL;
    flux_txn_begin_read(s, &t);
    flux_iter_t* it = NULL;
    flux_scan(t, NULL, &it);
    int n = 0;
    flux_record_t r;
    while (flux_iter_next(it, &r) == FLUX_OK) { n++; flux_record_free(&r); }
    flux_iter_free(it);

    printf("FLUXmeme v%s\n", fluxmeme_version());
    printf("  path        : %s\n", path);
    printf("  commit_seq  : %llu\n", (unsigned long long)flux_commit_seq(s));
    printf("  live records: %d\n", n);
    flux_close(s);
    return 0;
}

static int cmd_dump(const char* path) {
    flux_store_t* s = NULL;
    if (flux_open(path, 0, &s) != FLUX_OK) {
        printf("error: %s\n", flux_last_error());
        return 1;
    }
    flux_txn_t* t = NULL;
    flux_txn_begin_read(s, &t);
    flux_iter_t* it = NULL;
    flux_scan(t, NULL, &it);
    flux_record_t r;
    while (flux_iter_next(it, &r) == FLUX_OK) {
        char hex[33];
        flux_id_to_hex(&r.id, hex);
        printf("%s  %-7s %-12s %s\n", hex, layer_name(r.layer),
               r.kind ? r.kind : "", r.path ? r.path : "");
        if (r.payload.len) payload_preview(&r);
        flux_record_free(&r);
    }
    flux_iter_free(it);
    flux_close(s);
    return 0;
}

static int cmd_transcode(const char* path, const char* fmt, const char* out) {
    flux_store_t* s = NULL;
    if (flux_open(path, 0, &s) != FLUX_OK) {
        printf("error: %s\n", flux_last_error());
        return 1;
    }
    flux_txn_t* t = NULL;
    flux_txn_begin_read(s, &t);
    flux_status_t st;
    if (strcmp(fmt, "okf") == 0) st = flux_to_okf(t, out);
    else if (strcmp(fmt, "a2a") == 0) st = flux_to_a2a(t, out);
    else if (strcmp(fmt, "usd") == 0) st = flux_to_usd(t, out);
    else { printf("unknown format: %s (okf|a2a|usd)\n", fmt); flux_close(s); return 1; }
    if (st != FLUX_OK) printf("error: %s\n", flux_last_error());
    else printf("ok: %s -> %s (%s)\n", path, out, fmt);
    flux_close(s);
    return st == FLUX_OK ? 0 : 1;
}

/* SimReady -> DevReady: ingest a SimReady asset (USDA) into a fresh .flux,
 * turning a scene-only USD asset into a FLUXmeme embodied-node store. */
static int cmd_simready(const char* usda, const char* out_flux) {
    flux_store_t* s = NULL;
    if (flux_open(out_flux, 1, &s) != FLUX_OK) { printf("error: %s\n", flux_last_error()); return 1; }
    flux_txn_t* t = NULL;
    flux_txn_begin_write(s, &t);
    flux_status_t st = flux_from_usd(usda, t);
    flux_txn_commit(t);
    if (st != FLUX_OK) { printf("error: %s\n", flux_last_error()); flux_close(s); return 1; }
    /* report live record count */
    flux_txn_t* r = NULL;
    flux_txn_begin_read(s, &r);
    flux_iter_t* it = NULL;
    flux_scan(r, NULL, &it);
    int n = 0;
    flux_record_t rec;
    while (flux_iter_next(it, &rec) == FLUX_OK) { n++; flux_record_free(&rec); }
    flux_iter_free(it);
    flux_txn_rollback(r);
    printf("SimReady -> DevReady: %s -> %s (%d records)\n", usda, out_flux, n);
    flux_close(s);
    return 0;
}

static int cmd_compose(int argc, char** argv) {
    const char* root = argv[2];
    flux_compose_t* c = NULL;
    if (flux_compose_open(root, &c) != FLUX_OK) {
        printf("error: %s\n", flux_last_error());
        return 1;
    }
    printf("layers: %zu\n", flux_compose_n_layers(c));
    for (int i = 3; i < argc; ++i) {
        if (strncmp(argv[i], "--variant=", 10) == 0) {
            char buf[80];
            snprintf(buf, sizeof(buf), "%s", argv[i] + 10);
            char* eq = strchr(buf, '=');
            if (eq) { *eq = '\0'; flux_compose_set_variant(c, buf, eq + 1); }
        }
    }
    flux_compose_iter_t* it = NULL;
    if (flux_compose_scan(c, NULL, &it) != FLUX_OK) { flux_compose_close(c); return 1; }
    flux_record_t r;
    int n = 0;
    while (flux_compose_iter_next(it, &r) == FLUX_OK) {
        char hex[33];
        flux_id_to_hex(&r.id, hex);
        printf("%s  %-7s %-12s %s\n", hex, layer_name(r.layer),
               r.kind ? r.kind : "", r.path ? r.path : "");
        flux_record_free(&r);
        n++;
    }
    flux_compose_iter_free(it);
    flux_compose_close(c);
    printf("(%d merged records)\n", n);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("flux v%s — FLUXmeme CLI\n", fluxmeme_version());
        printf("usage:\n");
        printf("  flux inspect <file>\n");
        printf("  flux dump <file>\n");
        printf("  flux transcode <file> <okf|a2a|usd> <out>\n");
        printf("  flux conv <in> <out>   # .flux <-> .fluxa (by extension)\n");
        printf("  flux compose <root> [--variant S=V]...  # LIVRPS merged view\n");
        printf("  flux from-simready <usda> <out.flux>    # SimReady -> DevReady\n");
        return 1;
    }
    if (strcmp(argv[1], "inspect") == 0) return cmd_inspect(argv[2]);
    if (strcmp(argv[1], "dump") == 0) return cmd_dump(argv[2]);
    if (strcmp(argv[1], "compose") == 0) return cmd_compose(argc, argv);
    if (strcmp(argv[1], "from-simready") == 0 && argc >= 4) return cmd_simready(argv[2], argv[3]);
    if (strcmp(argv[1], "transcode") == 0 && argc >= 5) return cmd_transcode(argv[2], argv[3], argv[4]);
    if (strcmp(argv[1], "conv") == 0 && argc >= 4) {
        const char* in = argv[2];
        const char* out = argv[3];
        size_t inl = strlen(in), outl = strlen(out);
        int in_fluxa = inl > 6 && strcmp(in + inl - 6, ".fluxa") == 0;
        int in_flux = inl > 5 && strcmp(in + inl - 5, ".flux") == 0;
        int out_fluxa = outl > 6 && strcmp(out + outl - 6, ".fluxa") == 0;
        int out_flux = outl > 5 && strcmp(out + outl - 5, ".flux") == 0;
        if (in_flux && out_fluxa) {
            /* binary -> text */
            flux_store_t* s = NULL;
            if (flux_open(in, 0, &s) != FLUX_OK) { printf("error: %s\n", flux_last_error()); return 1; }
            flux_txn_t* t = NULL; flux_txn_begin_read(s, &t);
            flux_status_t st = flux_conv_to_fluxa(t, out);
            flux_close(s);
            printf("%s -> %s %s\n", in, out, st == FLUX_OK ? "ok" : flux_last_error());
            return st == FLUX_OK ? 0 : 1;
        }
        if (in_fluxa && out_flux) {
            /* text -> binary */
            flux_store_t* s = NULL;
            if (flux_open(out, 1, &s) != FLUX_OK) { printf("error: %s\n", flux_last_error()); return 1; }
            flux_txn_t* t = NULL; flux_txn_begin_write(s, &t);
            flux_status_t st = flux_conv_from_fluxa(in, t);
            flux_txn_commit(t);
            flux_close(s);
            printf("%s -> %s %s\n", in, out, st == FLUX_OK ? "ok" : flux_last_error());
            return st == FLUX_OK ? 0 : 1;
        }
        printf("conv needs one .flux and one .fluxa\n");
        return 1;
    }
    printf("unknown command or wrong args. run with no args for usage.\n");
    return 1;
}
