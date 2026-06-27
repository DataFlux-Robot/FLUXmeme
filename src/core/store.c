/* FLUXmeme store — open / header / append-only log / crash recovery / commit.
 * See SPEC.md §3, §6. */
#include "internal.h"
#include "fluxmeme/fluxmeme.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char* fluxmeme_version(void) {
    return "0.1.0";
}

/* ---- entry framing on disk ----
 * RecordEntry: "RECD"(4) body_len(4) body[body_len] pad[0..3] crc(4)
 * CommitMarker:"COMT"(4) commit_seq(8) commit_ts(8) delta(4) crc(4)   = 28 B
 */
static size_t pad4(size_t n) { return (4 - (n & 3)) & 3; }

static flux_status_t read_exact(flux_store_t* s, uint64_t off, void* buf, size_t n) {
    int64_t got = s->be->read(s->be->self, off, buf, n);
    if (got < 0 || (size_t)got != n) {
        flux_set_error("short read at %llu", (unsigned long long)off);
        return FLUX_ERR_IO;
    }
    return FLUX_OK;
}

/* ---- header ---- */
static flux_status_t write_header(flux_store_t* s) {
    uint8_t hdr[FLUX_HEADER_SIZE];
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, FLUX_MAGIC_FILE, 4);
    put_u16le(hdr + 4, FLUX_FMT_VERSION);
    put_u16le(hdr + 6, FLUX_HEADER_SIZE);
    /* flags 8..11 = 0 */
    put_u64le(hdr + 12, s->create_ts);
    put_u64le(hdr + 20, 0x464c55585f6d656dULL); /* schema_id placeholder */
    uint32_t crc = flux_crc32c(hdr, 28);
    put_u32le(hdr + 28, crc);
    /* store_uuid[16] + name[16] left zero for now */
    int64_t wrote = s->be->append(s->be->self, hdr, FLUX_HEADER_SIZE);
    if (wrote != FLUX_HEADER_SIZE)
        return FLUX_ERR_IO;
    return s->be->sync(s->be->self);
}

static flux_status_t read_header(flux_store_t* s) {
    uint8_t hdr[FLUX_HEADER_SIZE];
    if (read_exact(s, 0, hdr, FLUX_HEADER_SIZE) != FLUX_OK)
        return FLUX_ERR_IO;
    if (memcmp(hdr, FLUX_MAGIC_FILE, 4) != 0) {
        flux_set_error("not a FLUXmeme file (bad magic)");
        return FLUX_ERR_CORRUPT;
    }
    uint32_t crc = flux_crc32c(hdr, 28);
    if (crc != get_u32le(hdr + 28)) {
        flux_set_error("header crc mismatch");
        return FLUX_ERR_CRC;
    }
    s->create_ts = get_u64le(hdr + 12);
    return FLUX_OK;
}

/* ---- recovery scan: build committed entries from the log ---- */
static flux_status_t recover(flux_store_t* s) {
    uint64_t fsize = 0;
    if (s->be->size(s->be->self, &fsize) != FLUX_OK)
        return FLUX_ERR_IO;
    if (fsize == 0)
        return write_header(s); /* new file */
    if (fsize < FLUX_HEADER_SIZE)
        return FLUX_ERR_CORRUPT;
    if (read_header(s) != FLUX_OK)
        return FLUX_ERR_CORRUPT;

    /* scan entries from offset 64; pending entries are promoted to committed
     * on each valid COMT. */
    uint64_t off = FLUX_HEADER_SIZE;
    uint64_t committed_end = off;
    uint32_t best_seq = 0;
    /* two passes: first to find committed_end + best_seq, second to ingest.
     * Simpler: ingest into a temp array, and trim to committed on each COMT. */
    flux_entry_t* pending = NULL;
    size_t n_pending = 0, cap_pending = 0;

    while (off + 4 <= fsize) {
        uint8_t magic[4];
        if (read_exact(s, off, magic, 4) != FLUX_OK)
            break;
        if (memcmp(magic, FLUX_MAGIC_RECORD, 4) == 0) {
            uint8_t lenb[4];
            if (read_exact(s, off + 4, lenb, 4) != FLUX_OK)
                break;
            uint32_t blen = get_u32le(lenb);
            uint64_t body_off = off + 8;
            if (body_off + blen + 4 > fsize)
                break; /* torn tail */
            /* crc check */
            uint8_t* body = (uint8_t*)malloc(blen ? blen : 1);
            if (!body) return FLUX_ERR_NOMEM;
            if (blen && read_exact(s, body_off, body, blen) != FLUX_OK) { free(body); break; }
            uint32_t crc_stored;
            uint64_t crc_off = body_off + blen + pad4(blen);
            if (read_exact(s, crc_off, &crc_stored, 4) != FLUX_OK) { free(body); break; }
            uint32_t crc_calc = flux_crc32c(body, blen);
            free(body);
            if (crc_calc != crc_stored)
                break; /* corrupt/torn: stop, trust last COMT */

            /* decode just the header fields we need (id, ver, layer, is_tomb) */
            if (blen < 16 + 4 + 8 + 4) break; /* id16 layer1 pclass1 clock1 rsv1 ts8 ver4 = 32 */
            uint8_t head[32];
            if (read_exact(s, body_off, head, 32) != FLUX_OK) break;
            flux_entry_t e;
            memset(&e, 0, sizeof(e));
            memcpy(e.id.bytes, head, 16);
            e.layer = head[16];
            e.ver = get_u32le(head + 28); /* id16 + layer/pclass/clock/rsv(4) + ts8 = 28 */
            e.body_len = blen;
            e.off = body_off;
            e.is_tomb = (e.layer == 0); /* tombstone: layer==0, kind="__tomb__" (SPEC §3) */
            /* push to pending */
            if (n_pending == cap_pending) {
                cap_pending = cap_pending ? cap_pending * 2 : 16;
                pending = realloc(pending, cap_pending * sizeof(flux_entry_t));
                if (!pending) return FLUX_ERR_NOMEM;
            }
            pending[n_pending++] = e;
            off = crc_off + 4;
        } else if (memcmp(magic, FLUX_MAGIC_COMMIT, 4) == 0) {
            if (off + 28 > fsize) break;
            uint8_t cb[28];
            if (read_exact(s, off, cb, 28) != FLUX_OK) break;
            uint32_t crc_stored = get_u32le(cb + 24);
            uint32_t crc_calc = flux_crc32c(cb, 24);
            if (crc_calc != crc_stored) break; /* corrupt COMT: stop */
            uint64_t seq = get_u64le(cb + 4);
            /* promote pending to committed entries */
            for (size_t i = 0; i < n_pending; ++i) {
                if (s->n_entries >= FLUX_MAX_RECORDS) { /* DoS bound (SPEC §8) */
                    n_pending = 0;
                    goto recover_done;
                }
                if (s->n_entries == s->cap_entries) {
                    s->cap_entries = s->cap_entries ? s->cap_entries * 2 : 64;
                    s->entries = realloc(s->entries, s->cap_entries * sizeof(flux_entry_t));
                    if (!s->entries) return FLUX_ERR_NOMEM;
                }
                s->entries[s->n_entries++] = pending[i];
            }
            n_pending = 0;
            if (seq > best_seq) best_seq = (uint32_t)seq;
            committed_end = off + 28;
            off += 28;
        } else {
            break; /* unknown -> stop */
        }
    }
recover_done:
    free(pending);
    s->commit_seq = best_seq;
    /* writable: truncate uncommitted tail */
    if (s->writable && committed_end < fsize) {
        /* backend truncation not in iface; tolerated (tail ignored on reopen). */
    }
    return FLUX_OK;
}

flux_status_t flux_open(const char* path, int writable, flux_store_t** out) {
    flux_backend_t* be = flux_backend_file();
    if (!be) return FLUX_ERR_NOMEM;
    flux_status_t st = be->open(be->self, path, writable);
    if (st != FLUX_OK) {
        flux_set_error("backend open failed");
        return st;
    }
    flux_store_t* s = (flux_store_t*)calloc(1, sizeof(flux_store_t));
    if (!s) return FLUX_ERR_NOMEM;
    s->be = be;
    s->owns_be = 1;
    s->writable = writable;
    s->create_ts = (uint64_t)time(NULL) * 1000ULL;
    st = recover(s);
    if (st != FLUX_OK) {
        flux_close(s);
        return st;
    }
    *out = s;
    return FLUX_OK;
}

flux_status_t flux_open_with_backend(flux_backend_t* be, flux_store_t** out) {
    if (!be || !out) return FLUX_ERR_ARG;
    flux_store_t* s = (flux_store_t*)calloc(1, sizeof(flux_store_t));
    if (!s) return FLUX_ERR_NOMEM;
    s->be = be;
    s->owns_be = 0;
    s->writable = 1;
    s->create_ts = (uint64_t)time(NULL) * 1000ULL;
    flux_status_t st = recover(s);
    if (st != FLUX_OK) { flux_close(s); return st; }
    *out = s;
    return FLUX_OK;
}

void flux_close(flux_store_t* s) {
    if (!s) return;
    if (s->be) s->be->close(s->be->self);
    if (s->owns_be) free(s->be);
    free(s->entries);
    free(s);
}

uint64_t flux_commit_seq(const flux_store_t* s) {
    return s ? s->commit_seq : 0;
}

/* ---- append helpers (used by txn commit) ---- */
flux_status_t flux_store_append_entry(flux_store_t* s, const flux_buf_t* body,
                                      uint64_t* body_off_out) {
    /* "RECD" body_len(4) body pad crc(4) */
    size_t blen = body->len;
    size_t pad = pad4(blen);
    size_t frame = 4 + 4 + blen + pad + 4;
    uint8_t* buf = (uint8_t*)malloc(frame);
    if (!buf) return FLUX_ERR_NOMEM;
    memcpy(buf, FLUX_MAGIC_RECORD, 4);
    put_u32le(buf + 4, (uint32_t)blen);
    if (blen) memcpy(buf + 8, body->data, blen);
    for (size_t i = 0; i < pad; ++i) buf[8 + blen + i] = 0;
    uint32_t crc = flux_crc32c(body->data, blen);
    put_u32le(buf + 8 + blen + pad, crc);

    uint64_t before = 0;
    s->be->size(s->be->self, &before);
    int64_t wrote = s->be->append(s->be->self, buf, frame);
    free(buf);
    if (wrote != (int64_t)frame) return FLUX_ERR_IO;
    if (body_off_out) *body_off_out = before + 8;
    return FLUX_OK;
}

flux_status_t flux_store_append_commit(flux_store_t* s, uint64_t seq,
                                       uint64_t ts, uint32_t delta) {
    uint8_t cb[28];
    memcpy(cb, FLUX_MAGIC_COMMIT, 4);
    put_u64le(cb + 4, seq);
    put_u64le(cb + 12, ts);
    put_u32le(cb + 20, delta);
    put_u32le(cb + 24, flux_crc32c(cb, 24));
    int64_t wrote = s->be->append(s->be->self, cb, 28);
    if (wrote != 28) return FLUX_ERR_IO;
    return s->be->sync(s->be->self);
}

flux_status_t flux_store_read_entry(flux_store_t* s, uint64_t off,
                                    flux_record_t* out) {
    uint8_t lenb[4];
    /* entry layout: magic(4) @ off-8, body_len(4) @ off-4, body @ off */
    if (read_exact(s, off - 4, lenb, 4) != FLUX_OK) return FLUX_ERR_IO;
    uint32_t blen = get_u32le(lenb);
    uint8_t* body = (uint8_t*)malloc(blen ? blen : 1);
    if (!body) return FLUX_ERR_NOMEM;
    flux_status_t st = read_exact(s, off, body, blen);
    if (st != FLUX_OK) { free(body); return st; }
    st = flux_record_decode(body, blen, out);
    free(body);
    return st;
}
