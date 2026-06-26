# FLUXmeme — Format Specification (v1.0-draft)

> **FLUXmeme** is a self-describing asset format for **embodied nodes**.
> One `.flux` file = one embodied node (a robot) = its **body + mind + lifetime journal** in a single artifact, readable/writable from a 64-bit desktop down to a Cortex-M7 MCU, byte-for-byte identical across all platforms.

This document is the normative specification. The reference C implementation lives in this repo.

---

## 1. Essence

A robot today is scattered across 5+ mutually-unaware sidecar files (URDF body, USD scene, markdown docs, JSON agent card, MCAP/MAVLink logs, YAML config) that drift apart over its life. FLUXmeme unifies them: **one robot = one `.flux`**, with three *natures* mapped to three *layers*:

| `layer` | Nature | Facets (examples) | Transcodes to |
|---|---|---|---|
| `BODY` | physical structure | graph kinematics (closed-loop), geometry/inertia, **affordance**, device-comm topology | USD |
| `MIND` | knowledge + skills | OKF markdown (`kind=concept`), A2A cards/tasks (`kind=agent_card`/`task`) | OKF + A2A |
| `JOURNAL` | lifetime log | telemetry, params, **PHM** black-box slices | MCAP (primary) + MAVLink |

FLUXmeme is **decentralized by construction**: each `.flux` is one autonomous, self-rooted node; multi-node worlds are composed (no global scene root). The format is **dumb storage**: it stores only typed records of TEXT or BIN payloads + structured metadata; all parsing/rendering/solving is the application layer's job.

## 2. Record — the universal atom

Every facet is one **Record** of the same shape:

| Field | Type | Meaning |
|---|---|---|
| `id` | 16 B | ULID-like (48-bit ms + 70-bit random); sorted + globally unique |
| `content_hash` | 32 B | blake3 of the record body (dedup / integrity / provenance) |
| `path` | string | human-readable in-layer path (e.g. `body/links/base`) |
| `layer` | u8 | `BODY=1` / `MIND=2` / `JOURNAL=4` (bitset) |
| `pclass` | u8 | `TEXT=1` / `BIN=2` |
| `ptype` | string | mime hint, e.g. `text/markdown`, `application/json`, `model/stl`, `model/usda` (hint only; storage is dumb) |
| `kind` | string | canonical (`link`,`joint`,`mesh`,`concept`,`agent_card`,`phm_slice`,`signal`,…) or open |
| `meta` | KV[] | OKF-style frontmatter (type/title/description/resource/tags/…) |
| `links` | edge[] | directed graph edges `{target_id, rel}` — the knowledge/structure graph |
| `payload` | bytes | TEXT (UTF-8) or BIN — the actual content |
| `ts` | u64 | timestamp |
| `clock` | u8 | clock domain: `sim_time`/`wall_time`/`device_monotonic` (JOURNAL records) |
| `ver` | u32 | MVCC version = the `commit_seq` that produced this record |

**Canonical facets** (`robot-graph`, `affordance`, `device-comm`, `PHM`, `signal`, `calib`, `multifield`) carry versioned strong schemas; non-canonical `kind`/`meta` are fully open. Adding a new domain = adding a `kind`, never touching the engine.

## 3. On-disk layout

Little-endian, 4-byte aligned (records zero-padded). CRC = CRC32C. A `.flux` file is an **append-only log** + **incremental footer index** + **footer** (Parquet/Arrow-IPC style: cheap appends; MCU reads the small index first, then random-accesses one record — never the whole file).

```
[FileHeader  64 B]  magic "FLXM" | fmt_version(u16)=1 | header_size(u16)=64 | flags(u32)
                    | create_ts(u64) | schema_id(u64, flatcc schema hash) | header_crc(u32)
                    | store_uuid(16 B) + ascii_name(16 B)
[RecordEntry]*      sentinel "RECD" + rec_len(u32) + Record body + body_crc(u32)  (4-aligned)
[CommitMarker]*     sentinel "COMT" + commit_seq(u64) + commit_ts(u64) + delta(u32) + crc(u32)   ← per txn
[FooterIndex]*      incremental block: sorted id→offset (binary search) + tag→idlist
[Footer]            next_footer_off(u64) | root_index_off(u64) | magic "FLXF" | total_crc(u32)
```

- **Tombstone**: a RecordEntry with `layer=0, kind="__tomb__"`, carrying `(target_id)` in `meta`; readers skip tombstoned ids.
- **Crash safety / MVCC**: the visible version = the highest `commit_seq` among CRC-valid `COMT`s. Each record's `ver = commit_seq`. Any tail bytes after the last good `COMT` are truncated on next open.
- **Self-describing**: `magic "FLXM"` + `schema_id` (+ optional embedded schema) let `flux inspect` identify any file without a runtime.

## 4. Dual representation

| `.fluxa` | `.flux` |
|---|---|
| Human-readable text, USDA-like parentheses (`def Record "x" { … }`) | flatcc binary |
| **Canonical source** (git-friendly: diff/PR/review) | compiled artifact (`flux conv`) |
| What humans/agents author | what machines/MCUs run |

`flux conv` converts losslessly both ways; CI asserts the two stay equivalent.

## 5. Composition (LIVRPS)

A `.flux` may carry **composition arcs** (`reference` / `payload` / `inherit` / `specialize` / `variant`) and declare `subLayers[]` (strongest first). Reading resolves a **virtual merged view** by opinion strength — **L**ocal > **I**nherits > **V**ariants > **R**eferences > **P**ayloads > **S**pecializes — field-level merge, deterministic, non-destructive. Any node may act as a composition root (`flux compose`); composition is read-time/temporary/initiator-defined — no fixed authority, no sync runtime.

## 6. Concurrency

Append-only log + **MVCC**: lock-free multi-reader snapshots (a reader captures a `commit_seq` and sees all records with `ver ≤ snapshot` minus live tombstones); a single serialized writer appends records + a `COMT`, fsyncs, and atomically bumps the visible `commit_seq`. Multi-writer apps use optimistic **CAS** (`expected_ver`) or a single-writer thread pool. FIFO consumers use independent cursors over the log. Every version carries an `author` mark; configurable N historical versions enable `flux revert`.

## 7. Scope (infrastructure only)

FLUXmeme is **only** a format + read/write/maintain SDK. It deliberately does **not** do: decentralized sync/distribution, runtime scheduling, agent orchestration, an MCP server, FK/IK/dynamics solving, or application logic — those are upper layers built on top of the SDK. The SDK exposes primitives (composition, MVCC, author/N-version, content-addressing, transcoders, frame codec, CLI); *how to use them* is the user's call.

## 8. Conformance & evolution

- Semantic versioning; **fields are add-only** (flatcc forward/backward compat).
- Deprecated fields follow a deprecation cycle; `schema_id` + embedded schema let old tools self-identify new versions.
- Parsers treat all input (`.flux`/`.fluxa`/USDA/MAVLink) as **untrusted**: caps on record count, nesting depth, composition-arc recursion (cycle-break), and payload size.
