# Architecture

FLUXmeme is a **self-describing asset format for embodied nodes**: one `.flux` =
one robot = **body (BODY) + mind (MIND) + lifetime journal (JOURNAL)**. See
[SPEC.md](../SPEC.md) for the design rationale.

## Three tiers

```mermaid
graph TD
    subgraph "Tier 3 — Platform SDKs"
        PY[Python<br/>ctypes]
        CLI[flux CLI]
        FUT[Rust/JS/ROS2<br/>future]
    end
    subgraph "Tier 2 — C Middleware (one codebase, MCU budget)"
        PORT[port/HAL]
        CORE[Core Engine<br/>append log + MVCC + CRC]
        CODEC[Codec<br/>5 transcoders + conv]
        COMP[Composition<br/>LIVRPS merge]
    end
    subgraph "Tier 1 — Identical Format"
        FLUX[.flux binary]
        FLUXA[.fluxa text]
    end
    PY --> CORE
    CLI --> CORE
    FUT --> CORE
    PORT --> CORE
    CORE --> CODEC
    CORE --> COMP
    CORE <--> FLUX
    CORE <--> FLUXA
```

The **format is invariant**; platform differences are absorbed entirely by
Tier 2/3. Tier 2 is a single pure-C codebase; its complexity budget is "runs on
a Cortex-M7 (FLUXLOOP/STM32H7)".

## One source, many projections

```mermaid
graph LR
    FLUX[".flux<br/>(BODY + MIND + JOURNAL)"]
    FLUX -->|to_usd| USD[USD scene]
    FLUX -->|to_okf| OKF[OKF markdown]
    FLUX -->|to_a2a| A2A[A2A agent card]
    FLUX -->|to_mcap| MCAP[MCAP rosbag2]
    FLUX -->|to_mavlink| MAV[MAVLink frames]
    FLUX -->|to_fluxa| FLUXA[".fluxa text<br/>(Git-friendly)"]
```

FLUXmeme doesn't "contain" a USD file — it *is* the asset and renders a view on
demand. Your tools keep working; FLUXmeme is the single source behind them.

## Record structure

```mermaid
graph TD
    REC["Record (the universal atom)"]
    REC --> ID["id: ULID (16B)<br/>stable identity"]
    REC --> LAYER["layer: BODY / MIND / JOURNAL<br/>(bitset)"]
    REC --> KIND["kind: robot/link, concept, signal, ...<br/>(canonical or open)"]
    REC --> META["meta: KV dict<br/>(name, title, type, tags, ...)"]
    REC --> LINKS["links: directed edges<br/>(knowledge graph)"]
    REC --> PAYLOAD["payload: TEXT or BIN<br/>(markdown / JSON / mesh / signal)"]
    REC --> TS["ts + clock<br/>(multi-clock-domain timestamp)"]
    REC --> VER["ver: MVCC version<br/>(commit_seq)"]
```

Adding a new domain is a new `kind`, never an engine change.

## Composition (LIVRPS)

```mermaid
graph TD
    ROOT["root.flux<br/>(declares sublayers)"]
    L1["override.flux<br/>(strongest)"]
    L2["base.flux<br/>(weakest)"]
    ROOT --> L1
    ROOT --> L2
    L1 --> MERGE["Merged View<br/>(field-level merge<br/>+ variants + inherit)"]
    L2 --> MERGE
    MERGE --> USD["to_usd()"]
    MERGE --> OKF["to_okf()"]
```

Composition is **read-time, non-destructive**. Stronger layers override field by
field; weaker layers fill gaps. The original files are never modified.

## Lifecycle (DevReady)

```mermaid
graph LR
    GEN["1. GENERATE<br/>.flux created<br/>(body + mind)"]
    REUSE["2. REUSE<br/>project to USD/OKF<br/>feed sim + agents"]
    OPERATE["3. OPERATE<br/>append PHM signals<br/>(journal grows)"]
    REPLAY["4. REPLAY<br/>export MCAP<br/>(rosbag2)"]
    GEN --> REUSE --> OPERATE --> REPLAY
    OPERATE -.->|"same file"| GEN
```

One `.flux` through the full lifecycle. The journal grows on-device (MCU); the
body + mind stay constant. **Born in reality, perpetually real.**

## Backends

```mermaid
graph TD
    ENGINE["Core Engine<br/>(format + MVCC + codec)"]
    BE["backend_t abstraction"]
    FILE["flux_backend_file<br/>(portable stdio + flock)"]
    MMAP["flux_backend_mmap<br/>(desktop, planned)"]
    FLASH["flux_backend_flash<br/>(MCU/FlashDB, planned)"]
    ENGINE --> BE
    BE --> FILE
    BE --> MMAP
    BE --> FLASH
    FILE --> DISK1["same .flux bytes"]
    MMAP --> DISK2["same .flux bytes"]
    FLASH --> DISK3["same .flux bytes"]
```

Same bytes across all backends. A `.flux` written on a cloud GPU is read
unchanged on a Cortex-M7.
