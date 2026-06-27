#!/usr/bin/env python3
"""FLUXmeme + Newton integration demo.

Demonstrates:
  0. pip-installed FLUXmeme SDK (import fluxmeme)
  1. USD -> .flux   (ingest a SimReady/Newton asset into DevReady)
  2. .flux -> USD   (project back; round-trip)
  3. .flux advantages over plain USD (BODY + MIND in one file)
  4. Load the flux-exported USDA into Newton (if installed)

Usage:
  python demo/demo_newton.py                    # standalone (no Newton needed)
  python demo/demo_newton.py --newton           # with Newton (if installed)
  python -m newton.examples robot_g1            # run Newton's own example
"""
import os
import sys
import math

from fluxmeme import Store, Record, LAYER_BODY, LAYER_MIND, LAYER_JOURNAL

HERE = os.path.dirname(os.path.abspath(__file__))
ASSET = os.path.join(HERE, "assets", "cartpole.usda")
FLUX_FILE = os.path.join(HERE, "cartpole.flux")
FLUXA_FILE = os.path.join(HERE, "cartpole.fluxa")
USD_OUT = os.path.join(HERE, "cartpole_from_flux.usda")
MCAP_OUT = os.path.join(HERE, "cartpole_run.mcap")

# ────────────────────────────────────────────────────────────────────
# 1. USD -> .flux   (ingest a Newton/SimReady asset)
# ────────────────────────────────────────────────────────────────────
print("=" * 60)
print("1. USD -> .flux  (ingest a SimReady/Newton asset)")
print("=" * 60)

for f in [FLUX_FILE, USD_OUT, MCAP_OUT]:
    if os.path.exists(f):
        os.remove(f)

with Store(FLUX_FILE, writable=True) as s:
    # BODY: ingest the USDA meshes
    with s.write() as txn:
        s.from_usd(txn, ASSET)

    with s.read() as txn:
        body = list(s.scan(txn, layer=LAYER_BODY))
    print(f"   [OK] Ingested {len(body)} BODY records (meshes) from {os.path.basename(ASSET)}")

    # ── .flux ADVANTAGE: add knowledge that USD can't carry ──
    with s.write() as txn:
        s.put(txn, Record(
            layer=LAYER_MIND, kind="concept",
            meta={"title": "Cart-pole balancing task",
                  "tags": "rl,control,balance"},
            payload=b"# Cart-Pole Balancing\n\nBalance the pole upright by applying horizontal force to the cart.\nReward: keep pole angle near vertical.",
        ))
        s.put(txn, Record(
            layer=LAYER_MIND, kind="agent_card",
            ptype="application/json",
            payload=b'{"name":"ppo_balancer","version":"1.0","skill":"balance"}',
        ))
    print(f"   [OK] Added 2 MIND records (task concept + agent card)")
    print(f"   -> USD alone would only have {len(body)} meshes; .flux has BODY + MIND")

# ────────────────────────────────────────────────────────────────────
# 2. .flux -> USD   (project back; round-trip)
# ────────────────────────────────────────────────────────────────────
print()
print("=" * 60)
print("2. .flux -> USD  (project BODY back; round-trip)")
print("=" * 60)

with Store(FLUX_FILE) as s, s.read() as txn:
    s.to_usd(txn, USD_OUT)
    body = list(s.scan(txn, layer=LAYER_BODY))
    mind = list(s.scan(txn, layer=LAYER_MIND))

print(f"   [OK] Exported BODY -> {os.path.basename(USD_OUT)}")
print(f"   [OK] .flux contains: {len(body)} BODY + {len(mind)} MIND")
print(f"   -> The .flux is richer than the USD: it has knowledge (MIND)")
print(f"     that the USD projection doesn't carry")

# Also show .fluxa (text form) for the "GitHub-native" story
with Store(FLUX_FILE) as s, s.read() as txn:
    s.to_fluxa(txn, FLUXA_FILE)
print(f"   [OK] Also exported .fluxa (text canonical source, {os.path.getsize(FLUXA_FILE)} bytes)")

# ────────────────────────────────────────────────────────────────────
# 3. .flux advantages summary
# ────────────────────────────────────────────────────────────────────
print()
print("=" * 60)
print("3. .flux file advantages (vs plain USD)")
print("=" * 60)

with Store(FLUX_FILE) as s, s.read() as txn:
    layers = {LAYER_BODY: 0, LAYER_MIND: 0, LAYER_JOURNAL: 0}
    for r in s.scan(txn):
        for mask, cnt in layers.items():
            if r.layer & mask:
                layers[mask] += 1
    for mask in [LAYER_BODY, LAYER_MIND, LAYER_JOURNAL]:
        name = {1: "BODY", 2: "MIND", 4: "JOURNAL"}[mask]
        print(f"   {name:8s}: {layers[mask]} records")

print(f"""
   USD (cartpole.usda):        {os.path.getsize(ASSET):>6d} bytes -- scene only
   .flux (cartpole.flux):      {os.path.getsize(FLUX_FILE):>6d} bytes -- scene + knowledge + ready for journal
   .fluxa (cartpole.fluxa):    {os.path.getsize(FLUXA_FILE):>6d} bytes -- human-readable, diff-friendly

   Key advantages:
   * One file = body (scene) + mind (task knowledge) + journal (telemetry)
   * Agent-readable without a runtime (self-describing)
   * Diff-friendly text form (.fluxa) for Git workflows
   * Projects to USD/OKF/A2A/MCAP/MAVLink on demand
""")

# ────────────────────────────────────────────────────────────────────
# 4. Newton integration (optional -- if Newton is installed)
# ────────────────────────────────────────────────────────────────────
use_newton = "--newton" in sys.argv
if use_newton:
    print("=" * 60)
    print("4. Newton: load the flux-exported USDA")
    print("=" * 60)
    try:
        import newton
        import warp as wp

        builder = newton.ModelBuilder()
        builder.add_usd(USD_OUT)
        model = builder.finalize()
        solver = newton.solvers.SolverMuJoCo(model)
        state_0 = model.state()
        state_1 = model.state()
        control = model.control()
        newton.eval_fk(model, model.joint_q, model.joint_qd, state_0)

        # Run a few sim steps + log telemetry to .flux JOURNAL
        with Store(FLUX_FILE, writable=True) as s:
            dt = 1.0 / 240.0
            for step in range(100):
                state_0.clear_forces()
                solver.step(state_0, state_1, control, None, dt)
                state_0, state_1 = state_1, state_0

                # Log every 10th step as a JOURNAL signal
                if step % 10 == 0:
                    q = state_0.body_q.numpy()
                    with s.write() as txn:
                        s.put(txn, Record(
                            layer=LAYER_JOURNAL, kind="signal",
                            clock=0, ts=int(step * dt * 1e9),
                            meta={"name": "cart_x", "value": f"{q[0, 0]:.4f}"},
                        ))

        with Store(FLUX_FILE) as s, s.read() as txn:
            journal = list(s.scan(txn, layer=LAYER_JOURNAL))
            s.to_mcap(txn, MCAP_OUT)
        print(f"   [OK] Newton ran 100 steps, logged {len(journal)} signals to .flux JOURNAL")
        print(f"   [OK] Exported JOURNAL -> {os.path.basename(MCAP_OUT)} (MCAP/rosbag2)")
        print(f"   -> Run the full robot: python -m newton.examples robot_g1")

    except ImportError:
        print("   (Newton not installed. Install with: pip install newton-physics warp-lang)")
else:
    print("=" * 60)
    print("4. Newton (skipped -- run with --newton to enable)")
    print("=" * 60)
    print("   The flux-exported USDA is ready for:")
    print("     pip install newton-physics warp-lang")
    print("     python demo/demo_newton.py --newton")
    print("     python -m newton.examples robot_g1")

print()
print("[OK] Demo complete. Key takeaway:")
print("  .flux = USD (body) + knowledge (mind) + telemetry (journal) in ONE file.")
print("  Convert freely: .flux <-> USD, .flux -> OKF/A2A/MCAP/MAVLink.")
