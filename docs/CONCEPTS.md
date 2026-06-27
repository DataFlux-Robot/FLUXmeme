# FLUXmeme Concepts

Deep-dive guide to the ideas behind DevReady assets. If you're new, read
[Getting Started](GETTING_STARTED.md) first.

---

## 1. Why DevReady (not just SimReady)

NVIDIA's SimReady made 3D assets **physically accurate** — meshes carry measured
mass, friction, deformation. That solved **scene fidelity** for simulation.

But a modern robot is not a scene. It is an **embodied node** that also has:

- **Knowledge** — what to do (task specs, procedures)
- **Agency** — skills it can call (agent cards, API endpoints)
- **A lifetime health record** — what happened to it (telemetry, PHM)

SimReady carries none of these. They live in sidecar files (markdown, JSON,
MCAP, YAML) that drift apart, can't be composed, and don't run on the robot.

**DevReady** unifies them: one `.flux` = body + mind + journal, from cloud to MCU.

| Aspect | SimReady (USD) | DevReady (.flux) |
|---|---|---|
| What it carries | scene + physics | scene + **knowledge + agent + lifetime journal** |
| Agent-native | no (needs external runtime) | **yes** — self-describing; `flux inspect` |
| Composition | USD layers (scene-centric) | **LIVRPS, decentralized** (each robot = autonomous node) |
| Robot model | tree (URDF) | **graph, closed loops first-class** |
| Edge / MCU | no (USD is desktop-only) | **yes** — lean C core on Cortex-M7 |
| Lifecycle | born in sim, dies in sim | **born in reality, perpetually real** |

---

## 2. The three layers (BODY / MIND / JOURNAL)

A `.flux` holds three natures, each mapping to a standard format:

```
+---------------------------------------------------+
|                   .flux file                       |
|                                                   |
|  BODY    → USD        graph kinematics + geometry  |
|  MIND    → OKF + A2A  task knowledge + agent skills|
|  JOURNAL → MCAP + MAV telemetry + PHM black-box   |
|                                                   |
+---------------------------------------------------+
```

### BODY (the physical structure)

Holds the robot's **physical description**: links, joints, constraints (as a
**general graph**, not a URDF tree), geometry meshes, inertial properties,
device-comm topology, and affordances.

- `kind = "robot/link"` — a rigid body (meta: name, mass)
- `kind = "robot/joint"` — an edge (meta: type=revolute|prismatic|fixed, lower, upper, axis)
- `kind = "mesh"` — geometry (BIN payload, model/stl)
- `kind = "device-comm/node"` — a bus/device (meta: protocol, baud, addr)

**Closed loops** (4-bar, Delta, Stewart) are first-class: `flux_robot_has_cycle()`
returns 1. URDF can't express these; `.flux` does it naturally.

Transcodes to: **USD** (`flux_to_usd`).

### MIND (the knowledge + agency)

Holds what the robot **knows** and **can do**:

- `kind = "concept"` — task knowledge as OKF markdown (meta: title, tags)
- `kind = "agent_card"` — an A2A agent card (JSON payload)
- `kind = "task"` — a callable skill (linked `part_of` to its card)

An LLM/VLA reads MIND directly: `flux_to_okf()` produces a markdown bundle that
an agent can ingest for zero-shot task understanding.

Transcodes to: **OKF** + **A2A**.

### JOURNAL (the lifetime log)

An append-only log that **grows for the robot's entire life**:

- `kind = "signal"` — a telemetry sample (meta: name, value)
- `kind = "param"` — a configuration parameter
- `kind = "phm_slice"` — a PHM (Prognostics & Health Management) time slice

Each record carries a **multi-clock-domain timestamp** (`clock` field):
`sim_time`, `wall_time`, or `device_monotonic` — so sim and real-world telemetry
can be aligned unambiguously.

Transcodes to: **MCAP** (rosbag2, the primary projection) + **MAVLink** (for
edge/MCU transport; large assets are automatically filtered off the bus).

---

## 3. The Record — one atom, infinite facets

Every piece of data in a `.flux` is a **Record** with the same shape:

```
id (ULID) + layer + kind + meta (KV) + links (edges) + payload (TEXT|BIN)
         + ts + clock + ver (MVCC)
```

**Why one shape?** Because the storage layer is **dumb**: it stores bytes + typed
metadata. All parsing, rendering, FK/IK solving, scheduling — that's the
application layer. Adding a new domain (e.g., a new sensor type) is a new `kind`,
never an engine change.

This is the same philosophy as Unix ("everything is a file") applied to robot
assets: everything is a Record.

---

## 4. Composition (LIVRPS)

FLUXmeme borrows USD's **composition arcs** but decentralizes them:

### Layer stacks

A root `.flux` declares `sublayers` (a list of layer paths, strongest first):

```
root.flux → declares sublayers = [override.flux, base.flux]
```

### Field-level merge

When the same record `id` exists in multiple layers, **fields merge**:
- The strongest layer that provides a field **wins**
- Weaker layers **fill gaps** the stronger doesn't have

Example: base has `{name, mass=10, color=red}`; override has `{mass=20}`.
Merged = `{name, mass=20, color=red}` — mass overridden, color survives.

### Variants

A record can be tagged with `flux_variant_set=S, flux_variant=V`. Activate a
variant with `flux_compose_set_variant(c, "S", "V")`:

- Tagged records with matching set+value become **active** (stronger than base)
- Other tagged records are **hidden**
- Untagged records are **always active** (the base)

### Inherit

A record with `flux_inherit=<base_id>` pulls the base record's fields as
**weaker** — fills gaps the inheriting record doesn't have. One level deep.

### Non-destructive

Composition is a **read-time virtual merge**. The original layer files are
**never modified**. `flux_compose_open(root)` returns a view; close it and the
layers are unchanged.

---

## 5. Transcoders as projections

A transcoder is a **view**, not the thing itself. FLUXmeme doesn't "contain a
USD file" — it **is** the asset and renders a USD view on demand.

```
                 ┌─── to_usd() ──────── USD scene (BODY meshes)
                 ├─── to_okf() ──────── OKF markdown (MIND concepts)
.flux ───────────┤─── to_a2a() ──────── A2A agent card (MIND skills)
                 ├─── to_mcap() ─────── MCAP rosbag2 (JOURNAL signals)
                 ├─── to_mavlink() ──── MAVLink frames (JOURNAL signals)
                 └─── to_fluxa() ────── .fluxa text (whole store, Git-friendly)
```

Each `from_*` reverses the projection. This means your existing tools (Newton,
Isaac Sim, Gazebo) keep working — they consume the standard they already speak.
FLUXmeme is the **single source** behind them.

---

## 6. Self-description (agent-native)

A `.flux` is **self-describing**:

- Magic bytes `FLXM` + schema_id in the header — `flux inspect` identifies any file
- Records carry `kind` + `meta` (human-readable key/value) — an agent can
  understand the content without a runtime
- The `.fluxa` text form is **grep-able** and **diff-able** — agents read it like
  any markdown file

This is the "agent-native" property: an AI agent (LLM, VLA) can open a `.flux`,
scan its records, understand what the robot is and what it can do — **without
installing a simulation runtime or a USD plugin**.

---

## 7. The lifecycle: born in reality, perpetually real

```
GENERATE → REUSE → OPERATE → (loop)
```

1. **Generate**: a `.flux` is born (design → graph body; add task knowledge; add
   agent skills). Can be from URDF/SDF/USD import or built from scratch.

2. **Reuse**: the same `.flux` projects to USD for sim, OKF for VLA prompting,
   A2A for multi-agent orchestration. No sidecar drift.

3. **Operate**: the same `.flux` accrues telemetry (PHM signals, health data) in
   its JOURNAL — on the real device (MCU), growing over the robot's life.

4. **Replay**: the JOURNAL exports to MCAP for post-hoc analysis; the BODY+MIND
   stay constant. One artifact, the whole story.

**The bet**: the asset that accompanies an embodied agent through its entire life
should be **one artifact**, not a folder that drifts apart.

---

## 8. Dumb storage, smart application

The most important design principle:

> **The format stores only TEXT/BIN payloads + structured metadata.
> Parsing, rendering, solving, scheduling — all stay in the application layer.**

This is why the core is small enough for an MCU (Cortex-M7). It's also why
adding a new domain is a new `kind` — not a schema change, not an engine rebuild.

Compare to USD: USD's engine understands prims, attributes, composition arcs,
relationships, instancing. That's powerful but heavy (desktop-only). FLUXmeme's
engine understands Records. That's simple but universal (cloud-to-MCU).

---

## Further reading

- [SPEC.md](../SPEC.md) — the normative format specification
- [FORMAT.md](FORMAT.md) — binary byte layout
- [API.md](API.md) — complete C + Python reference
- [ARCHITECTURE.md](ARCHITECTURE.md) — three-tier diagrams
- [GETTING_STARTED.md](GETTING_STARTED.md) — step-by-step tutorial
