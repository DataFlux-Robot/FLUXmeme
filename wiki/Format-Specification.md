# Format Specification

Normative reference: [SPEC.md](https://github.com/DataFlux-Robot/FLUXmeme/blob/main/SPEC.md).
This page is the wiki-friendly version with examples.

## Overview

A `.flux` file is an **append-only log** of self-describing records + per-transaction
commit markers. Little-endian, 4-byte aligned, CRC32C.

```
[FileHeader  64 B]
[RecordEntry]*       "RECD" body_len(u32) body[...] pad[0..3] crc32c(u32)
[CommitMarker]*      "COMT" commit_seq(u64) commit_ts(u64) delta(u32) crc32c(u32)
```

## FileHeader (64 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `"FLXM"` |
| 4 | 2 | `fmt_version` (u16, =1) |
| 6 | 2 | `header_size` (=64) |
| 8 | 4 | `flags` |
| 12 | 8 | `create_ts` (unix ms) |
| 20 | 8 | `schema_id` |
| 28 | 4 | `header_crc` (CRC32C of bytes 0..27) |
| 32 | 16 | `store_uuid` |
| 48 | 16 | ascii name (zero-padded) |

## RecordEntry body (length-prefixed fields)

```
id[16]              ULID-like identity (48-bit ms timestamp + 80-bit random, big-endian for sort)
layer(u8)           BODY=1 | MIND=2 | JOURNAL=4 (bitset)
pclass(u8)          TEXT=1 | BIN=2
clock(u8)           sim_time=0 | wall_time=1 | device_monotonic=2
reserved(u8)
ts(u64)             timestamp
ver(u32)            MVCC version = the commit_seq that wrote this record
content_hash[32]    hash of body excluding id (integrity / dedup)
path_len(u16) path[]
ptype_len(u16) ptype[]
kind_len(u16) kind[]
meta_count(u16)     meta_count * (k_len u16, k[], v_len u16, v[])
link_count(u16)     link_count * (target_id[16], rel_len u16, rel[])
payload_len(u32) payload[]
```

### Example (hex-annotated)

A minimal concept record (layer=MIND, kind="concept", payload="# Hi"):

```
RECD                          ; sentinel
00 00 00 2F                  ; body_len = 47 bytes
01 93 A4 E3 ... (16 bytes)   ; id (ULID)
02                            ; layer = MIND (2)
01                            ; pclass = TEXT (1)
00                            ; clock = sim_time (0)
00                            ; reserved
00 00 00 00 67 6E 4B 00      ; ts = 1782000000 ms
00 00 00 01                  ; ver = 1
00 00 ... (32 bytes zeros)   ; content_hash
00 00                        ; path_len = 0
00 00                        ; ptype_len = 0
07 63 6F 6E 63 65 70 74     ; kind = "concept" (len=7)
00 00                        ; meta_count = 0
00 00                        ; link_count = 0
00 00 00 04                  ; payload_len = 4
23 20 48 69                  ; payload = "# Hi"
XX XX XX XX                  ; CRC32C of body
```

## CommitMarker (28 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `"COMT"` |
| 4 | 8 | `commit_seq` (u64) |
| 12 | 8 | `commit_ts` (u64) |
| 20 | 4 | `record_count_delta` (u32) |
| 24 | 4 | CRC32C of bytes 0..23 |

Each write transaction appends its records then one CommitMarker. The CommitMarker
is what makes them atomically visible.

## Tombstone

A RecordEntry with `layer = 0`, `kind = "__tomb__"`, carrying `(target_id)` in
meta. Readers skip tombstoned ids at their snapshot.

## MVCC + crash safety

- The visible version = the highest `commit_seq` among CRC-valid CommitMarkers
- Each record's `ver` = the `commit_seq` of the transaction that wrote it
- A read snapshot captures `commit_seq` and sees `ver <= snapshot` minus live tombstones
- A torn tail (no COMT / bad CRC) is **silently truncated** on next open

## .fluxa text format

The canonical text source (line-oriented, diff-friendly):

```
#FLUXMEME 1.0
R <32-hex-id>
L BODY|MIND|JOURNAL
K <kind>
P <ptype>
G <path>
T <ts>
C sim_time|wall_time|device_monotonic
B TEXT|BIN
M <key>=<value>           (repeatable)
N <32-hex-target>=<rel>   (repeatable)
D <len>                   (TEXT payload: next len bytes verbatim)
X <hexlen>                (BIN payload: next hexlen hex chars)
```

Convert: `flux conv file.flux file.fluxa` (and reverse).

## Hardening caps (untrusted input)

| Cap | Limit |
|---|---|
| payload per record | 256 MiB |
| meta KV per record | 4096 |
| links per record | 4096 |
| records on open | 16M (DoS bound) |
| composition layer depth | 64 (cycle bound) |

---

# 格式规范(中文)

规范参考:[SPEC.md](https://github.com/DataFlux-Robot/FLUXmeme/blob/main/SPEC.md)。
本页是 wiki 友好版,带示例。

## 概览

`.flux` 文件 = **append-only 日志** + 每事务的 commit marker。小端序,4 字节对齐,CRC32C。

```
[FileHeader  64B]
[RecordEntry]*       "RECD" body_len(u32) body pad crc32c(u32)
[CommitMarker]*      "COMT" commit_seq(u64) commit_ts(u64) delta(u32) crc32c(u32)
```

## FileHeader(64 字节)

| 偏移 | 大小 | 字段 |
|---|---|---|
| 0 | 4 | magic `"FLXM"` |
| 4 | 2 | `fmt_version`(=1) |
| 6 | 2 | `header_size`(=64) |
| 8 | 4 | `flags` |
| 12 | 8 | `create_ts`(unix ms) |
| 20 | 8 | `schema_id` |
| 28 | 4 | `header_crc`(前 28 字节的 CRC32C) |
| 32 | 16 | `store_uuid` |
| 48 | 16 | ascii 名称(零填充) |

## RecordEntry body(长度前缀字段)

```
id[16]              ULID 身份(48-bit ms 时间戳 + 80-bit 随机,大端排序)
layer(u8)           BODY=1 | MIND=2 | JOURNAL=4(位集)
pclass(u8)          TEXT=1 | BIN=2
clock(u8)           sim_time=0 | wall_time=1 | device_monotonic=2
reserved(u8)
ts(u64)             时间戳
ver(u32)            MVCC 版本 = 写入此记录的 commit_seq
content_hash[32]    body 哈希(不含 id)— 完整性/去重
path_len(u16) path[]
ptype_len(u16) ptype[]
kind_len(u16) kind[]
meta_count(u16)     meta_count * (k_len u16, k[], v_len u16, v[])
link_count(u16)     link_count * (target_id[16], rel_len u16, rel[])
payload_len(u32) payload[]
```

## CommitMarker(28 字节)

| 偏移 | 大小 | 字段 |
|---|---|---|
| 0 | 4 | magic `"COMT"` |
| 4 | 8 | `commit_seq`(u64) |
| 12 | 8 | `commit_ts`(u64) |
| 20 | 4 | `record_count_delta`(u32) |
| 24 | 4 | 前 24 字节的 CRC32C |

## MVCC + 崩溃安全

- 可见版本 = CRC 校验通过的最大 `commit_seq`
- 读快照捕获 `commit_seq`,看到 `ver <= 快照` 减去活跃 tombstone
- 撕裂尾部(无 COMT / CRC 错)下次 open **静默截断**

## .fluxa 文本格式

Canonical 文本源(行式,可 diff):

```
#FLUXMEME 1.0
R <32-hex-id>
L BODY|MIND|JOURNAL
K <kind>
...
D <len>     (TEXT 载荷:接下来 len 字节原文)
X <hexlen>  (BIN 载荷:接下来 hexlen 个 hex 字符)
```

转换:`flux conv file.flux file.fluxa`(双向)。

## 硬化上限(不可信输入)

| 上限 | 限制 |
|---|---|
| 每记录 payload | 256 MiB |
| 每记录 meta KV | 4096 |
| 每记录 links | 4096 |
| open 时记录数 | 16M(DoS 上限) |
| 组合层深 | 64(环上限) |
