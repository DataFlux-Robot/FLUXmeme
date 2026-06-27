/* MCAP (rosbag2) transcoder — JOURNAL/signal <-> a minimal valid .mcap file.
 *
 * MCAP v1 framing: magic | records... | magic, where each record is
 * opcode(u8) + length(u64 LE) + payload. Minimal non-chunked stream:
 *   Header(0x01) Schema(0x0c) Channel(0x04) Message*(0x05) DataEnd(0x46) Footer(0x02)
 * Message data = "name=value\n" per JOURNAL signal (channel = raw text).
 * See SPEC §0.3 (JOURNAL -> MCAP primary). */
#include "fluxmeme/fluxmeme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* MCAP v1 magic header/footer (8 bytes): 0x89 'M' 'C' 'A' 'P' \r \n \n */
static const unsigned char MAGIC[8] = {0x89, 0x4D, 0x43, 0x41, 0x50, 0x0D, 0x0A, 0x0A};

static void w_u16(FILE* f, uint16_t v) { uint8_t b[2] = {(uint8_t)v,(uint8_t)(v>>8)}; fwrite(b,1,2,f); }
static void w_u32(FILE* f, uint32_t v) { uint8_t b[4]; for(int i=0;i<4;++i) b[i]=(uint8_t)(v>>(8*i)); fwrite(b,1,4,f); }
static void w_u64(FILE* f, uint64_t v) { uint8_t b[8]; for(int i=0;i<8;++i) b[i]=(uint8_t)(v>>(8*i)); fwrite(b,1,8,f); }
static void w_str(FILE* f, const char* s) { size_t n=strlen(s); w_u32(f,(uint32_t)n); fwrite(s,1,n,f); }
/* write a record: opcode + u64 len + payload */
static void w_rec(FILE* f, uint8_t op, const void* payload, size_t len) {
    fputc(op, f); w_u64(f, (uint64_t)len);
    if (len) fwrite(payload, 1, len, f);
}

static const char* meta_val(const flux_record_t* r, const char* key) {
    for (uint32_t i = 0; i < r->meta_count; ++i)
        if (r->meta[i].key && strcmp(r->meta[i].key, key) == 0)
            return r->meta[i].val ? r->meta[i].val : "";
    return "";
}

flux_status_t flux_to_mcap(const flux_txn_t* txn, const char* out_mcap) {
    if (!txn || !out_mcap) return FLUX_ERR_ARG;
    FILE* f = fopen(out_mcap, "wb");
    if (!f) return FLUX_ERR_IO;
    fwrite(MAGIC, 1, 8, f);

    /* Header: profile, library */
    { char buf[128]; size_t n = 0;
      n += (size_t)snprintf(buf+n,sizeof(buf)-n,""); /* profile payload built below */
      /* build payload = profile_str + library_str */
      /* simpler: write fields directly into a temp buffer */
      unsigned char hdr[128]; size_t p = 0;
      const char* prof = "x-fer"; uint32_t pl = (uint32_t)strlen(prof);
      memcpy(hdr+p,&pl,4); p+=4; memcpy(hdr+p,prof,pl); p+=pl;
      const char* lib = "fluxmeme 0.1"; uint32_t ll = (uint32_t)strlen(lib);
      memcpy(hdr+p,&ll,4); p+=4; memcpy(hdr+p,lib,ll); p+=ll;
      w_rec(f, 0x01, hdr, p);
    }
    /* Schema(id=1) */
    { unsigned char s[128]; size_t p=0;
      uint16_t sid=1; memcpy(s+p,&sid,2); p+=2;
      const char* nm="flux.Signal"; uint32_t nl=(uint32_t)strlen(nm); memcpy(s+p,&nl,4); p+=4; memcpy(s+p,nm,nl); p+=nl;
      const char* enc="jsonschema"; uint32_t el=(uint32_t)strlen(enc); memcpy(s+p,&el,4); p+=4; memcpy(s+p,enc,el); p+=el;
      const char* def="{\"type\":\"object\"}"; uint32_t dl=(uint32_t)strlen(def); memcpy(s+p,&dl,4); p+=4; memcpy(s+p,def,dl); p+=dl;
      w_rec(f, 0x0c, s, p);
    }
    /* Channel(id=1, schema=1) */
    { unsigned char c[128]; size_t p=0;
      uint16_t cid=1; memcpy(c+p,&cid,2); p+=2;
      uint16_t sid=1; memcpy(c+p,&sid,2); p+=2;
      const char* topic="/flux/signals"; uint32_t tl=(uint32_t)strlen(topic); memcpy(c+p,&tl,4); p+=4; memcpy(c+p,topic,tl); p+=tl;
      const char* me=""; uint32_t ml=0; memcpy(c+p,&ml,4); p+=4; /* message_encoding empty(raw) */
      uint32_t mc=0; memcpy(c+p,&mc,4); p+=4; /* metadata count 0 */
      w_rec(f, 0x04, c, p);
    }

    /* Messages: one per JOURNAL signal */
    flux_iter_t* it = NULL;
    if (flux_scan(txn, NULL, &it) != FLUX_OK) { fclose(f); return FLUX_ERR_IO; }
    flux_record_t r;
    uint32_t seq = 0;
    uint64_t t0 = (uint64_t)time(NULL) * 1000000000ULL;
    while (flux_iter_next(it, &r) == FLUX_OK) {
        if (!(r.layer & FLUX_LAYER_JOURNAL) || !r.kind ||
            (strcmp(r.kind,"signal")!=0 && strcmp(r.kind,"param")!=0)) {
            flux_record_free(&r); continue;
        }
        const char* name = meta_val(&r, "name");
        const char* val = meta_val(&r, "value");
        char data[160];
        int dn = snprintf(data, sizeof(data), "%s=%s\n", name, val);
        if (dn < 0) dn = 0;
        unsigned char m[64]; size_t p=0;
        uint16_t cid=1; memcpy(m+p,&cid,2); p+=2;
        memcpy(m+p,&seq,4); p+=4; (void)seq;
        uint64_t lt = t0 + seq*1000000ULL; memcpy(m+p,&lt,8); p+=8;
        memcpy(m+p,&lt,8); p+=8; /* publish_time */
        uint32_t dl=(uint32_t)dn; memcpy(m+p,&dl,4); p+=4;
        unsigned char* rec = (unsigned char*)malloc(p + (size_t)dn);
        memcpy(rec, m, p); memcpy(rec+p, data, dn);
        w_rec(f, 0x05, rec, p + (size_t)dn);
        free(rec);
        seq++;
        flux_record_free(&r);
    }
    flux_iter_free(it);

    /* DataEnd (crc=0, disabled) */
    { uint32_t crc = 0; w_rec(f, 0x46, &crc, 4); }
    /* Footer (no summary: both offsets 0, crc 0) */
    { unsigned char fo[20]; memset(fo,0,sizeof(fo)); w_rec(f, 0x02, fo, 20); }

    fwrite(MAGIC, 1, 8, f);
    fclose(f);
    return FLUX_OK;
}

/* ---- read ---- */
static uint32_t r_u32(const uint8_t* p){ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static uint64_t r_u64(const uint8_t* p){ uint64_t v=0; for(int i=0;i<8;++i) v|=((uint64_t)p[i])<<(8*i); return v; }

flux_status_t flux_from_mcap(const char* in_mcap, flux_txn_t* txn) {
    if (!in_mcap || !txn) return FLUX_ERR_ARG;
    FILE* f = fopen(in_mcap, "rb");
    if (!f) return FLUX_ERR_IO;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buf = (uint8_t*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return FLUX_ERR_NOMEM; }
    size_t total = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (total < 16 || memcmp(buf, MAGIC, 8) != 0) { free(buf); return FLUX_ERR_CORRUPT; }

    size_t off = 8;
    int imported = 0;
    while (off + 9 <= total) {
        uint8_t op = buf[off];
        uint64_t len = r_u64(buf + off + 1);
        off += 9;
        if (off + len > total) break;
        const uint8_t* pl = buf + off;
        if (op == 0x05) { /* Message */
            /* channel_id(2) seq(4) log_time(8) publish_time(8) data_len(4) data */
            if (len >= 26) {
                uint32_t dl = r_u32(pl + 22);
                if (22 + 4 + dl <= len) {
                    const char* data = (const char*)(pl + 26);
                    /* parse "name=value\n" */
                    const char* eq = memchr(data, '=', dl);
                    if (eq) {
                        size_t nlen = (size_t)(eq - data);
                        const char* vp = eq + 1;
                        size_t vlen = dl - (nlen + 1);
                        while (vlen && (vp[vlen-1]=='\n'||vp[vlen-1]=='\r')) vlen--;
                        char nm[64], vl[64];
                        size_t cn = nlen < 63 ? nlen : 63; memcpy(nm, data, cn); nm[cn]=0;
                        size_t cv = vlen < 63 ? vlen : 63; memcpy(vl, vp, cv); vl[cv]=0;
                        flux_meta_kv_t m[2];
                        m[0].key="name"; m[0].val=nm; m[1].key="value"; m[1].val=vl;
                        flux_record_t r; memset(&r,0,sizeof(r));
                        r.layer = FLUX_LAYER_JOURNAL;
                        r.pclass = FLUX_PCLASS_TEXT;
                        r.kind = "signal";
                        r.ptype = "application/x-mcap";
                        r.meta = m; r.meta_count = 2;
                        r.clock = FLUX_CLOCK_WALL_TIME;
                        r.ts = (uint64_t)time(NULL) * 1000ULL;
                        flux_put(txn, &r);
                        imported++;
                    }
                }
            }
        }
        off += (size_t)len;
    }
    free(buf);
    return imported >= 0 ? FLUX_OK : FLUX_OK;
}
