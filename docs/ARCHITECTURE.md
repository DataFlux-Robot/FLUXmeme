# Architecture

FLUXmeme is a **self-describing asset format for embodied nodes**: one `.flux` =
one robot = **body (BODY) + mind (MIND) + lifetime journal (JOURNAL)**. See
[SPEC.md](../SPEC.md) §0 for the design rationale.

## Three tiers

```
[Tier 3] platform SDKs   Python (ctypes) | CLI (`flux`) | … (Rust/JS/ROS2 later)
            ↕
[Tier 2] C middleware     port/HAL + core engine + codec + compose   (one codebase)
            ↕
[Tier 1] identical format  .flux (binary) / .fluxa (text)             (byte-identical)
```

The **format is invariant**; platform differences are absorbed entirely by Tier 2/3.
Tier 2 is a single pure-C codebase; the Tier 2 complexity budget is "runs on a
Cortex-M7 (FLUXLOOP/STM32H7)" — that constraint is the razor keeping the core
honest and portable (heavy work — OpenUSD, URDF parsing, Python, full LIVRPS — is
Tier 3 only and never compiled into MCU firmware).

## Core engine (`src/core/`)

- **append-only log** of self-describing `RecordEntry`s + per-txn `CommitMarker`s
  → the log is *simultaneously* an asset container and a lifetime journal (the
  PHM black-box is just the log's growing tail).
- **MVCC**: a read txn captures a `commit_seq` snapshot (lock-free); a single
  serialized writer appends + a COMT + fsync. App-level multi-writers use
  optimistic `flux_cas`.
- **dumb storage**: the engine stores only TEXT/BIN payloads + structured
  metadata. Parsing/rendering/solving is the **application layer** — adding a new
  domain is a new `kind`, never an engine change.

## Codec / transcode (`src/transcode/`, `src/codec/`)

Transcoders are **projections**: they render the (composed) record view to/from
foreign formats — USD (BODY), OKF (MIND/concept), A2A (MIND/agent),
MAVLink (JOURNAL/signal, large assets filtered), URDF/SDF import, and `.fluxa`
text canonical source. They never fight existing standards; FLUXmeme is their
unified upstream.

## Composition (`src/compose/`)

A read-time, **non-destructive** merged view over a layer stack (root + declared
sublayers). LIVRPS strength order, field-level merge, and variant sets. No
central scene root, no sync runtime — composition is the decentralized,
read-time merge of immutable versioned layers.

## Backends (`src/port/`)

A `flux_backend_t` abstracts `open/read/append/sync/lock/unlock/size/close`; the
shipped `flux_backend_file` is portable stdio/Win32 + flock/LockFileEx. `mmap`
(desktop) and flash/FlashDB (embedded) are designed-for and added per-platform;
the on-disk format is identical across all of them.

## Robot facets (`src/facets/`)

- **robot-graph**: link/joint/constraint as a *general graph* (closed loops are
  first-class — the capability URDF's pure tree can't express); DFS cycle check.
- **device-comm**: device/bus topology with per-protocol routing — the addressing
  basis the MAVLink/CAN/EtherCAT frame codecs route over.
