#!/usr/bin/env python3
"""FLUXmeme hardware metadata demo — motor library + dynamics + inertial.

Demonstrates .flux covering URDF-Studio's hardware feature set:
  1. Import a URDF robot (links + joints)
  2. Enrich with motor/hardware metadata (from motor library)
  3. Add joint dynamics (damping, friction, stiffness)
  4. Add inertial properties (mass + inertia tensor)
  5. Add safety controller + calibration
  6. Export the hardware-enriched .flux

Usage:
  python demo/demo_hardware.py
"""
import json
import os
import sys

from fluxmeme import Store, Record, LAYER_BODY

HERE = os.path.dirname(os.path.abspath(__file__))
URDF = os.path.join(HERE, "assets", "quadruped.urdf")
MOTOR_LIB = os.path.join(HERE, "assets", "motor_library.json")
FLUX_OUT = os.path.join(HERE, "quadruped_hw.flux")

# ── Load motor library ──
with open(MOTOR_LIB) as f:
    lib = json.load(f)

# Pick motors for the quadruped's 8 joints (4 hips + 4 knees)
hip_motor = lib["motors"]["Unitree"]["Go1-M8010-6"]
knee_motor = lib["motors"]["DAMIAO"]["DM-J4310-2EC V1.1"]

print("=" * 60)
print("1. Import quadruped URDF -> .flux")
print("=" * 60)

if os.path.exists(FLUX_OUT):
    os.remove(FLUX_OUT)

with Store(FLUX_OUT, writable=True) as s:
    with s.write() as txn:
        s.from_urdf(txn, URDF)

    with s.read() as txn:
        joints = [r for r in s.scan(txn, layer=LAYER_BODY) if r.kind == "robot/joint"]
        links = [r for r in s.scan(txn, layer=LAYER_BODY) if r.kind == "robot/link"]
    print(f"   [OK] Imported: {len(links)} links + {len(joints)} joints")

    # ── 2. Enrich joints with motor/hardware metadata ──
    print()
    print("=" * 60)
    print("2. Enrich with motor/hardware metadata (from motor library)")
    print("=" * 60)

    with s.write() as txn:
        for j in joints:
            is_knee = "knee" in (j.meta.get("name", "") or "")
            motor = knee_motor if is_knee else hip_motor
            motor_name = "DM-J4310-2EC V1.1" if is_knee else "Go1-M8010-6"
            brand = "DAMIAO" if is_knee else "Unitree"

            # Update the joint with hardware metadata (same id = update)
            updated = Record(
                layer=LAYER_BODY,
                kind="robot/joint",
                id=j.id,
                meta={
                    **j.meta,
                    # Motor library reference
                    "motor_brand": brand,
                    "motor_type": motor_name,
                    "motor_armature": str(motor["armature"]),
                    "motor_velocity": str(motor["velocity"]),
                    "motor_effort": str(motor["effort"]),
                    # Per-joint hardware config (URDF-Studio style)
                    "motor_id": str(joints.index(j) + 1),  # CAN bus ID
                    "motor_direction": "1",
                    "hardware_interface": "effort",
                    # Joint dynamics
                    "damping": "0.1",
                    "friction": "0.01",
                    "stiffness": "0.0",
                    # Safety controller
                    "soft_lower": j.meta.get("lower", "-1.57"),
                    "soft_upper": j.meta.get("upper", "1.57"),
                    "k_position": "100.0",
                    "k_velocity": "10.0",
                    # Gear ratio
                    "gear_ratio": "6.0" if not is_knee else "9.1",
                },
                links=j.links,
            )
            s.put(txn, updated)

    print(f"   [OK] Enriched {len(joints)} joints with motor/dynamics/safety metadata")
    print(f"   Hip motor:  Unitree Go1-M8010-6 (armature={hip_motor['armature']}, effort={hip_motor['effort']} N*m)")
    print(f"   Knee motor: DAMIAO DM-J4310-2EC (armature={knee_motor['armature']}, effort={knee_motor['effort']} N*m)")

    # ── 3. Enrich links with inertial ──
    print()
    print("=" * 60)
    print("3. Add inertial properties (mass + inertia tensor)")
    print("=" * 60)

    with s.write() as txn:
        for i, link in enumerate(links):
            inertia_vals = {
                "base": {"mass": "5.0", "ixx": "0.05", "ixy": "0", "ixz": "0",
                          "iyy": "0.10", "iyz": "0", "izz": "0.12"},
                "arm": {"mass": "0.5", "ixx": "0.001", "ixy": "0", "ixz": "0",
                        "iyy": "0.002", "iyz": "0", "izz": "0.002"},
            }
            vals = inertia_vals.get("base" if i == 0 else "arm", inertia_vals["arm"])
            updated = Record(
                layer=LAYER_BODY,
                kind="robot/link",
                id=link.id,
                meta={**link.meta, **vals},
                links=link.links,
            )
            s.put(txn, updated)

    print(f"   [OK] Enriched {len(links)} links with mass + inertia tensor")

    # ── 4. Verify ──
    print()
    print("=" * 60)
    print("4. Verify: full hardware-enriched .flux")
    print("=" * 60)

    with s.read() as txn:
        joints = [r for r in s.scan(txn, layer=LAYER_BODY) if r.kind == "robot/joint"]
        for j in joints[:2]:  # show first 2 joints
            name = j.meta.get("name", j.id[:8])
            print(f"\n   Joint: {name}")
            print(f"     type:            {j.meta.get('type', '?')}")
            print(f"     limit:           [{j.meta.get('lower', '?')}, {j.meta.get('upper', '?')}]")
            print(f"     axis:            {j.meta.get('axis', '?')}")
            print(f"     motor:           {j.meta.get('motor_brand', '?')} {j.meta.get('motor_type', '?')}")
            print(f"     armature:        {j.meta.get('motor_armature', '?')} kg*m^2")
            print(f"     max_effort:      {j.meta.get('motor_effort', '?')} N*m")
            print(f"     max_velocity:    {j.meta.get('motor_velocity', '?')} rad/s")
            print(f"     gear_ratio:      {j.meta.get('gear_ratio', '?')}")
            print(f"     damping:         {j.meta.get('damping', '?')} N*m*s/rad")
            print(f"     friction:        {j.meta.get('friction', '?')} N*m")
            print(f"     hw_interface:    {j.meta.get('hardware_interface', '?')}")
            print(f"     motor_id (CAN):  {j.meta.get('motor_id', '?')}")
            print(f"     safety k_pos:    {j.meta.get('k_position', '?')}")
            print(f"     safety k_vel:    {j.meta.get('k_velocity', '?')}")

    # ── 5. Export ──
    print()
    print("=" * 60)
    print("5. Export the hardware-enriched .flux")
    print("=" * 60)

    with s.read() as txn:
        s.to_fluxa(txn, FLUX_OUT.replace(".flux", ".fluxa"))
    print(f"   [OK] Exported .fluxa (text canonical source)")

    print()
    print("[OK] Demo complete.")
    print("  The .flux now carries full hardware metadata (matching URDF-Studio):")
    print("  motor brand/type/armature/effort/velocity, gear ratio, damping/friction,")
    print("  safety controller, CAN ID + direction. All as structured meta KV.")
    print("  This is the DevReady advantage: hardware knowledge travels WITH the asset.")
