# Changelog

All notable changes to FLUXmeme are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/), and this project adheres to
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- MJCF (MuJoCo) import — niche format (URDF/SDF are the main paths)
- Real blake3 content hash (currently a deterministic placeholder)
- Embedded MCU port (STM32H7/FLUXLOOP) — requires embedded toolchain

## [0.1.0] — 2026-06-26

### Core engine
- Append-only log + MVCC (lock-free multi-reader, single serialized writer)
- Record wire codec with CRC32C, ULID identity, tombstones, content hash
- Portable file backend (stdio/Win32 + flock/LockFileEx)
- Optimistic CAS for multi-writer apps
- FIFO cursors for multi-consumer streaming
- `flux_compact` — reclaim append-only growth
- Untrusted-input hardening caps (payload/meta/links/records/layers)

### Format
- `.flux` binary (append-only log + commit markers + footer)
- `.fluxa` text canonical source (line-oriented, diff-friendly)
- `flux conv` — lossless bidirectional conversion

### Transcoders (5, all round-trip tested)
- USD (BODY: STL <-> Mesh, USDA parse/emit)
- OKF (MIND: concept <-> markdown bundle + frontmatter)
- A2A (MIND: agent_card/task <-> JSON bundle)
- MCAP (JOURNAL: signal <-> rosbag2 MCAP v1)
- MAVLink (JOURNAL: signal <-> MAVLink v2 frames, large-asset filter)

### Composition (LIVRPS)
- Layer stacks (sublayers) + field-level merge + variants
- Inherit arc (record-level class inheritance)
- Cycle/depth cap (untrusted-input bound)

### Robot facets
- robot-graph: closed-loop kinematics (DFS cycle detection, beyond URDF)
- device-comm: device/bus topology + per-protocol routing query
- URDF/SDF import (with limit/axis dynamics extraction)
- SimReady -> DevReady one-command conversion skill

### CLI
- `flux inspect|dump|transcode|conv|compose|from-simready`

### Python
- `pip install git+https://github.com/DataFlux-Robot/FLUXmeme.git`
- ctypes bindings (Store, Record, all transcoders + cursors + delete + compact)
- Validated with CPython 3.10 (pytest 2/2)

### Docs & governance
- SPEC.md (single-page format specification)
- docs/FORMAT.md, API.md, ARCHITECTURE.md, BENCHMARK.md, GETTING_STARTED.md
- CONTRIBUTING.md, CODE_OF_CONDUCT.md, SECURITY.md
- RFC process (docs/rfc/)
- Bilingual flagship README (English + Chinese)
- GitHub Actions CI (Windows MSVC + Linux gcc)

### Demos (12, all PASS on MSVC)
- demo_okf / demo_a2a / demo_usd / demo_mcap / demo_mavlink — transcoder round-trips
- demo_conv — .flux <-> .fluxa
- demo_robot — closed-loop (4-bar) vs tree (chain)
- demo_device — RS485/MAVLink topology + protocol routing
- demo_compose / demo_livrps — LIVRPS field merge + variants
- demo_urdf — URDF/SDF import + graph verification
- demo_lifecycle — hero: generate -> reuse -> operate in one .flux
- demo_newton — Newton integration (USD <-> .flux + advantages)

### Newton integration
- `demo/demo_newton.py` — conditional Newton sim (runs with or without Newton)
- Integration guide in README (Isaac Lab / Newton / UnitPort)
