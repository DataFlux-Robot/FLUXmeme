/* MAVLink v2 framing + the FLUX_PARAM transcoder (JOURNAL/signal <-> frames).
 *
 * Pure codec: produces/consumes frame bytes; does NOT open a serial socket or
 * schedule — the caller drives the bus (SPEC §6B). Only JOURNAL/signal records
 * go on the bus; large assets (mesh/OKF/agent) are filtered out. */
#include "mavlink.h"
#include "fluxmeme/fluxmeme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

uint16_t flux_mav_crc_accumulate(uint16_t crc, uint8_t b) {
    uint8_t tmp = b ^ (uint8_t)(crc & 0xff);
    tmp ^= tmp << 4;
    return (crc >> 8) ^ ((uint16_t)tmp << 8) ^ ((uint16_t)tmp << 3) ^ (tmp >> 4);
}

size_t flux_mav_encode(uint8_t sysid, uint8_t compid, uint16_t seq,
                       uint8_t msgid, uint8_t crc_extra,
                       const uint8_t* payload, uint8_t payload_len, uint8_t* out) {
    if (!out) return 0;
    /* header: STX LEN INCOMPAT COMPAT SEQ SYSID COMPID MSGID(3) = 10 bytes */
    out[0] = FLUX_MAVLINK_STX;
    out[1] = payload_len;
    out[2] = 0; /* incompat flags */
    out[3] = 0; /* compat flags */
    out[4] = (uint8_t)(seq & 0xff);
    out[5] = sysid;
    out[6] = compid;
    out[7] = (uint8_t)(msgid & 0xff);
    out[8] = (uint8_t)((msgid >> 8) & 0xff);
    out[9] = (uint8_t)((msgid >> 16) & 0xff);
    if (payload_len && payload) memcpy(out + 10, payload, payload_len);
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < (size_t)10 + payload_len; ++i)
        crc = flux_mav_crc_accumulate(crc, out[i]);
    crc = flux_mav_crc_accumulate(crc, crc_extra);
    out[10 + payload_len] = (uint8_t)(crc & 0xff);
    out[10 + payload_len + 1] = (uint8_t)((crc >> 8) & 0xff);
    return 10 + payload_len + 2;
}

int flux_mav_decode(const uint8_t* buf, size_t buf_len, size_t* plen,
                    uint8_t* sysid, uint8_t* compid, uint8_t* msgid,
                    uint8_t* payload, uint8_t* payload_len) {
    if (buf_len < 12) return 0;
    if (buf[0] != FLUX_MAVLINK_STX) return 0;
    uint8_t plenb = buf[1];
    size_t frame = 10 + plenb + 2;
    if (buf_len < frame) return 0;
    /* verify CRC */
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < (size_t)10 + plenb; ++i)
        crc = flux_mav_crc_accumulate(crc, buf[i]);
    /* CRC_EXTRA unknown to a generic decoder; FLUX_PARAM uses FLUX_MAVLINK_CRC_EXTRA */
    crc = flux_mav_crc_accumulate(crc, FLUX_MAVLINK_CRC_EXTRA);
    uint16_t stored = (uint16_t)buf[10 + plenb] | ((uint16_t)buf[10 + plenb + 1] << 8);
    if (crc != stored) return 0;
    if (sysid) *sysid = buf[5];
    if (compid) *compid = buf[6];
    if (msgid) *msgid = buf[7];
    if (payload_len) *payload_len = plenb;
    if (payload && plenb <= FLUX_PARAM_PAYLOAD_LEN) memcpy(payload, buf + 10, plenb);
    if (plen) *plen = frame;
    return 1;
}

/* ---- transcoder: JOURNAL/signal <-> FLUX_PARAM frames ---- */

static const char* meta_val(const flux_record_t* r, const char* key) {
    for (uint32_t i = 0; i < r->meta_count; ++i)
        if (r->meta[i].key && strcmp(r->meta[i].key, key) == 0)
            return r->meta[i].val ? r->meta[i].val : "";
    return "";
}

flux_status_t flux_to_mavlink(const flux_txn_t* txn, const char* out_frames) {
    if (!txn || !out_frames) return FLUX_ERR_ARG;
    FILE* f = fopen(out_frames, "wb");
    if (!f) return FLUX_ERR_IO;
    flux_iter_t* it = NULL;
    if (flux_scan(txn, NULL, &it) != FLUX_OK) { fclose(f); return FLUX_ERR_IO; }
    flux_record_t r;
    uint16_t seq = 0;
    while (flux_iter_next(it, &r) == FLUX_OK) {
        /* ONLY signal/param JOURNAL records go on the bus; large assets filtered */
        if (!(r.layer & FLUX_LAYER_JOURNAL) || !r.kind ||
            (strcmp(r.kind, "signal") != 0 && strcmp(r.kind, "param") != 0)) {
            flux_record_free(&r);
            continue;
        }
        uint8_t pl[FLUX_PARAM_PAYLOAD_LEN];
        memset(pl, 0, sizeof(pl));
        strncpy((char*)pl, meta_val(&r, "name"), 15);
        float v = (float)strtod(meta_val(&r, "value"), NULL);
        memcpy(pl + 16, &v, 4);
        uint8_t frame[40];
        size_t n = flux_mav_encode(1, 1, seq++, FLUX_MAVLINK_MSGID,
                                   FLUX_MAVLINK_CRC_EXTRA, pl, FLUX_PARAM_PAYLOAD_LEN, frame);
        if (n) fwrite(frame, 1, n, f);
        flux_record_free(&r);
    }
    flux_iter_free(it);
    fclose(f);
    return FLUX_OK;
}

flux_status_t flux_from_mavlink(const char* in_frames, flux_txn_t* txn) {
    if (!in_frames || !txn) return FLUX_ERR_ARG;
    FILE* f = fopen(in_frames, "rb");
    if (!f) return FLUX_ERR_IO;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buf = (uint8_t*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return FLUX_ERR_NOMEM; }
    size_t total = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    size_t off = 0;
    int imported = 0;
    while (off < total) {
        size_t plen = 0;
        uint8_t msgid = 0, payload[FLUX_PARAM_PAYLOAD_LEN] = {0}, payload_len = 0;
        if (!flux_mav_decode(buf + off, total - off, &plen, NULL, NULL, &msgid,
                             payload, &payload_len))
            break; /* not a frame / corrupt -> stop */
        if (msgid == FLUX_MAVLINK_MSGID && payload_len == FLUX_PARAM_PAYLOAD_LEN) {
            char name[17];
            memcpy(name, payload, 16);
            name[16] = '\0';
            float v;
            memcpy(&v, payload + 16, 4);
            char valbuf[32];
            snprintf(valbuf, sizeof valbuf, "%g", (double)v);
            flux_meta_kv_t m[2];
            m[0].key = "name";  m[0].val = name;
            m[1].key = "value"; m[1].val = valbuf;
            flux_record_t r;
            memset(&r, 0, sizeof(r));
            r.layer = FLUX_LAYER_JOURNAL;
            r.pclass = FLUX_PCLASS_TEXT;
            r.kind = "signal";
            r.ptype = "application/mavlink";
            r.meta = m;
            r.meta_count = 2;
            r.ts = (uint64_t)time(NULL) * 1000ULL;
            flux_put(txn, &r);
            imported++;
        }
        off += plen;
    }
    free(buf);
    return imported >= 0 ? FLUX_OK : FLUX_OK;
}
