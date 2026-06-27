# Getting Started with FLUXmeme

This tutorial takes you from zero to a working `.flux` DevReady asset in 10
minutes. You'll create a robot, add knowledge, run projections, and see why one
`.flux` beats a folder of sidecar files.

## Prerequisites

```bash
pip install git+https://github.com/DataFlux-Robot/FLUXmeme.git
```

Python >= 3.9. On Windows, run from a VS 2022 Developer prompt if the compiler
isn't found.

## Step 1: Create your first `.flux`

```python
from fluxmeme import Store, Record, LAYER_BODY

with Store("my_robot.flux", writable=True) as s:
    with s.write() as txn:
        # A robot link (rigid body)
        s.put(txn, Record(
            layer=LAYER_BODY,
            kind="robot/link",
            meta={"name": "base"},
        ))
```

That's it — you've created a `.flux` file. It's a self-describing, append-only
binary store. Inspect it:

```bash
python -c "from fluxmeme import Store; s=Store('my_robot.flux'); print('records:', s.commit_seq)"
```

## Step 2: The three layers

A `.flux` holds three natures. Add one record to each:

```python
from fluxmeme import Store, Record, LAYER_BODY, LAYER_MIND, LAYER_JOURNAL

with Store("my_robot.flux", writable=True) as s:
    with s.write() as txn:
        # BODY: the physical robot
        s.put(txn, Record(layer=LAYER_BODY, kind="robot/link", meta={"name": "arm"}))
        s.put(txn, Record(layer=LAYER_BODY, kind="robot/joint",
                          meta={"type": "revolute", "lower": "-3.14", "upper": "3.14"}))

        # MIND: what the robot knows
        s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                          meta={"title": "Pick and place"},
                          payload=b"# Pick objects from left, place on right."))

        # JOURNAL: what happened to it
        s.put(txn, Record(layer=LAYER_JOURNAL, kind="signal",
                          meta={"name": "battery", "value": "12.4"}))

    # Verify
    with s.read() as txn:
        for layer, name in [(LAYER_BODY,"BODY"), (LAYER_MIND,"MIND"), (LAYER_JOURNAL,"JOURNAL")]:
            n = len(list(s.scan(txn, layer=layer)))
            print(f"  {name:8s}: {n} records")
```

Output:
```
  BODY    : 3 records
  MIND    : 1 records
  JOURNAL : 1 records
```

All three natures in ONE file. That's the DevReady difference — no sidecar
drift.

## Step 3: Project to standard formats

A `.flux` is the single source. Project to whatever your tools consume:

```python
with Store("my_robot.flux") as s, s.read() as txn:
    s.to_usd(txn, "robot.usda")       # BODY -> USD scene
    s.to_okf(txn, "okf_out/")         # MIND -> OKF markdown bundle
    s.to_mcap(txn, "run.mcap")        # JOURNAL -> MCAP (rosbag2)
    s.to_mavlink(txn, "edge.frames")  # JOURNAL -> MAVLink frames
    s.to_fluxa(txn, "robot.fluxa")    # whole store -> text (for Git)
```

Your sim gets USD. Your LLM gets OKF markdown. Your MCU gets MAVLink. All from
one `.flux`. Zero drift.

## Step 4: Ingest an existing robot (URDF/USD)

Already have a robot in URDF or USD? One import:

```python
with Store("imported.flux", writable=True) as s:
    with s.write() as txn:
        s.from_urdf(txn, "my_robot.urdf")  # or: s.from_usd(txn, "scene.usda")
    # The robot's links + joints are now BODY records.
    # Add MIND knowledge on top:
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                          meta={"title": "Walking task"},
                          payload=b"# Walk forward at 0.5 m/s."))
```

Or from the CLI:

```bash
# SimReady USD -> DevReady .flux
flux from-simready scene.usda robot.flux
```

## Step 5: Compose layers (LIVRPS)

Override parameters non-destructively across layers:

```python
# Create a base robot
with Store("base.flux", writable=True) as s:
    with s.write() as txn:
        r = Record(layer=LAYER_BODY, kind="robot/link", meta={"name":"base","mass":"10"})
        s.put(txn, r)
        base_id = r.id

# Override mass in a stronger layer (same id)
with Store("override.flux", writable=True) as s:
    with s.write() as txn:
        r = Record(layer=LAYER_BODY, kind="robot/link", meta={"name":"base","mass":"20"})
        r.id = base_id  # same id -> override
        s.put(txn, r)

# Root declares the layer stack
with Store("root.flux", writable=True) as s:
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_MIND, kind="flux/compose",
                          meta={"sublayers": "override.flux;base.flux"}))
```

Resolve the merged view (override wins; base untouched):

```python
# CLI:
#   flux compose root.flux

# Or use the Newton demo as a full example:
#   python demo/demo_newton.py
```

## Step 6: The full lifecycle

The killer feature: one `.flux` through **generate -> reuse -> operate**:

```python
# 1. GENERATE: create a DevReady asset (body + mind)
with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.from_urdf(txn, "robot.urdf")       # body
        s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                          payload=b"# Pick task"))  # mind

# 2. REUSE: project to your tools
with Store("robot.flux") as s, s.read() as txn:
    s.to_usd(txn, "for_sim.usda")             # feed Newton/Isaac Sim
    s.to_okf(txn, "for_agent/")               # feed your VLA

# 3. OPERATE: append telemetry to the same file (journal grows)
with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_JOURNAL, kind="signal",
                          meta={"name": "joint_0", "value": "0.15"}))
    with s.read() as txn:
        s.to_mcap(txn, "run.mcap")             # export to MCAP for replay
```

**One file, born in reality, perpetually real.** That's DevReady.

## Next steps

- [SPEC.md](../SPEC.md) — the format specification
- [API.md](API.md) — complete C + Python API reference
- [ARCHITECTURE.md](ARCHITECTURE.md) — three-tier architecture + diagrams
- [FORMAT.md](FORMAT.md) — binary layout
- Run the demos: `python demo/demo_newton.py` (USD round-trip)
- Read the source: `include/fluxmeme/fluxmeme.h` (C API), `python/fluxmeme/api.py` (Python)
