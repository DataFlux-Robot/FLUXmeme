# `.flux` Binary Format (v1)

Normative reference: [SPEC.md](../SPEC.md). This document is the byte-level
companion. Little-endian; records 4-byte aligned (zero-padded). CRC = CRC32C
(Castagnoli). A `.flux` file is an append-only log:

```
[FileHeader  64 B]
[RecordEntry]*      (each: "RECD" body_len(u32) body[...] pad[0..3] crc32c(u32))
[CommitMarker]*     (each: "COMT" commit_seq(u64) commit_ts(u64) delta(u32) crc32c(u32))
```

## FileHeader (64 B)

| off | sz | field |
|---|---|---|
| 0 | 4 | magic `"FLXM"` |
| 4 | 2 | `fmt_version` (u16, =1) |
| 6 | 2 | `header_size` (=64) |
| 8 | 4 | `flags` |
| 12 | 8 | `create_ts` (unix ms) |
| 20 | 8 | `schema_id` |
| 28 | 4 | `header_crc` (over bytes 0..27) |
| 32 | 16 | `store_uuid` |
| 48 | 16 | ascii name (zero-padded) |

## RecordEntry body (length-prefixed fields, in order)

```
id[16] layer(u8) pclass(u8) clock(u8) rsv(u8)
ts(u64) ver(u32) content_hash[32]
path_len(u16) path[]  ptype_len(u16) ptype[]  kind_len(u16) kind[]
meta_count(u16)  meta_count*( k_len(u16) k[] v_len(u16) v[] )
link_count(u16)  link_count*( target_id[16] rel_len(u16) rel[] )
payload_len(u32) payload[]
```

`content_hash` = 32-byte hash over the body excluding the 16-byte `id` (the slot
is zeroed during hashing); two records with identical content (bar id) hash alike.

## CommitMarker (28 B)

`"COMT"` commit_seq(u64) commit_ts(u64) record_delta(u32) crc32c(u32) — crc over
the first 24 bytes. Each write transaction appends its records then one COMT; the
COMT is what makes them visible (atomic).

## MVCC + crash safety

The visible version = the highest `commit_seq` among CRC-valid `COMT`s. Each
record's `ver` = the `commit_seq` of the txn that wrote it. A read snapshot
captures a `commit_seq` and sees records with `ver ≤ snapshot` minus live
tombstones. A torn tail (no COMT / bad CRC) is ignored on open.

## Tombstone

A RecordEntry with `layer = 0`, `kind = "__tomb__"`, carrying `(target_id)` in
meta. Readers skip a tombstoned id.

## Notes / status

- The **footer index** (`id→offset`, binary-searchable) currently lives in RAM
  (rebuilt by scanning the log on open). On-disk footer persistence for MCU
  random access is a tracked item (SPEC §3); the in-memory index is O(N) open
  and fine for desktop/large assets.
- `content_hash` is a fast deterministic hash today; it drops to blake3 under the
  same `flux_hash(in, len, out[32])` interface (SPEC §2).
