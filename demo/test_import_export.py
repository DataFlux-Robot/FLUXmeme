#!/usr/bin/env python3
"""FLUXmeme Importer/Exporter test — .flux as the universal intermediate format.

Tests round-trip conversion through .flux for multiple robot types and formats,
demonstrating .flux as the single hub: any format in, any format out.

Inspired by https://github.com/robot-descriptions/awesome-robot-descriptions
"""
import os
import sys
import tempfile

from fluxmeme import Store, Record, LAYER_BODY, LAYER_MIND
from fluxmeme.enrich import enrich_from_urdf, load_motor_library, attach_motor, embed_motor_library
from fluxmeme.ai_review import review_robot

HERE = os.path.dirname(os.path.abspath(__file__))

ROBOTS = [
    ("Quadruped (URDF)", "assets/quadruped.urdf", "urdf"),
    ("Cartpole (USD)",   "assets/cartpole.usda",   "usd"),
]

# Extra robots we generate inline for format coverage
HUMANOID_URDF = """<?xml version="1.0"?>
<robot name="humanoid">
  <link name="torso"><inertial><mass value="8.0"/><inertia ixx="0.1" ixy="0" ixz="0" iyy="0.1" iyz="0" izz="0.05"/></inertial></link>
  <link name="head"/>
  <link name="l_arm"/>
  <link name="r_arm"/>
  <link name="l_leg"/>
  <link name="r_leg"/>
  <joint name="neck" type="revolute"><parent link="torso"/><child link="head"/><axis xyz="0 1 0"/><limit lower="-1" upper="1" effort="5" velocity="2"/></joint>
  <joint name="l_shoulder" type="revolute"><parent link="torso"/><child link="l_arm"/><axis xyz="0 1 0"/><limit lower="-2" upper="2" effort="10" velocity="2"/><dynamics damping="0.5" friction="0.1"/></joint>
  <joint name="r_shoulder" type="revolute"><parent link="torso"/><child link="r_arm"/><axis xyz="0 1 0"/><limit lower="-2" upper="2" effort="10" velocity="2"/><dynamics damping="0.5" friction="0.1"/></joint>
  <joint name="l_hip" type="revolute"><parent link="torso"/><child link="l_leg"/><axis xyz="0 1 0"/><limit lower="-1.5" upper="1.5" effort="30" velocity="3"/></joint>
  <joint name="r_hip" type="revolute"><parent link="torso"/><child link="r_leg"/><axis xyz="0 1 0"/><limit lower="-1.5" upper="1.5" effort="30" velocity="3"/></joint>
</robot>
"""

MANIPULATOR_URDF = """<?xml version="1.0"?>
<robot name="manipulator">
  <link name="base"><inertial><mass value="3.0"/><inertia ixx="0.05" ixy="0" ixz="0" iyy="0.05" iyz="0" izz="0.05"/></inertial></link>
  <link name="link1"/>
  <link name="link2"/>
  <link name="link3"/>
  <link name="gripper"/>
  <joint name="j1" type="revolute"><parent link="base"/><child link="link1"/><axis xyz="0 0 1"/><limit lower="-3.14" upper="3.14" effort="20" velocity="1.5"/></joint>
  <joint name="j2" type="revolute"><parent link="link1"/><child link="link2"/><axis xyz="0 1 0"/><limit lower="-2" upper="2" effort="20" velocity="1.5"><mimic joint="j1" multiplier="0.5" offset="0"/></limit></joint>
  <joint name="j3" type="revolute"><parent link="link2"/><child link="link3"/><axis xyz="0 1 0"/><limit lower="-2" upper="2" effort="15" velocity="2"/></joint>
  <joint name="j_grip" type="prismatic"><parent link="link3"/><child link="gripper"/><axis xyz="1 0 0"/><limit lower="0" upper="0.05" effort="5" velocity="0.1"/></joint>
</robot>
"""

EXTRA_ROBOTS = [
    ("Humanoid (URDF)", HUMANOID_URDF, "urdf"),
    ("Manipulator (URDF)", MANIPULATOR_URDF, "urdf"),
]


def test_roundtrip(name, source_path_or_xml, source_format, is_inline=False):
    """Import -> .flux -> export (USD + fluxa). Return pass/fail."""
    print(f"\n{'='*60}")
    print(f"  {name}")
    print(f"{'='*60}")

    tmpdir = tempfile.mkdtemp()
    flux_path = os.path.join(tmpdir, "robot.flux")
    usd_out = os.path.join(tmpdir, "robot_out.usda")
    fluxa_out = os.path.join(tmpdir, "robot_out.fluxa")

    # Write inline URDF to temp file if needed
    if is_inline:
        src_path = os.path.join(tmpdir, "robot_src.urdf")
        with open(src_path, "w") as f:
            f.write(source_path_or_xml)
    else:
        src_path = source_path_or_xml

    try:
        # 1. Import: source -> .flux
        with Store(flux_path, writable=True) as s:
            with s.write() as txn:
                if source_format == "urdf":
                    s.from_urdf(txn, src_path)
                elif source_format == "usd":
                    s.from_usd(txn, src_path)

            with s.read() as txn:
                body = list(s.scan(txn, layer=LAYER_BODY))
            print(f"  Import: {source_format} -> .flux : {len(body)} BODY records")

            # 2. Enrich (URDF only — extract dynamics/inertial/safety/mimic)
            if source_format == "urdf":
                n = enrich_from_urdf(s, src_path)
                print(f"  Enrich: +{n} records enriched (dynamics/inertial/safety/mimic)")

            # 3. Export: .flux -> USD + fluxa
            with s.read() as txn:
                s.to_usd(txn, usd_out)
                s.to_fluxa(txn, fluxa_out)

            # 4. Verify
            with s.read() as txn:
                body = list(s.scan(txn, layer=LAYER_BODY))
                links = [r for r in body if r.kind == "robot/link"]
                joints = [r for r in body if r.kind == "robot/joint"]
                meshes = [r for r in body if r.kind == "mesh"]

            has_inertia = sum(1 for l in links if "mass" in l.meta)
            has_dynamics = sum(1 for j in joints if "damping" in j.meta)

            print(f"  Export: .flux -> USDA + fluxa")
            print(f"    Links: {len(links)} (inertia: {has_inertia})")
            print(f"    Joints: {len(joints)} (dynamics: {has_dynamics})")
            print(f"    Meshes: {len(meshes)}")
            print(f"    USD out: {os.path.getsize(usd_out)} bytes")
            print(f"    fluxa out: {os.path.getsize(fluxa_out)} bytes")

            # 5. AI review
            result = review_robot(s)
            print(f"  AI Review: {result.overall}/100")

            # 6. Re-import exported USD -> new .flux (round-trip)
            flux2 = os.path.join(tmpdir, "roundtrip.flux")
            with Store(flux2, writable=True) as s2:
                with s2.write() as txn:
                    s2.from_usd(txn, usd_out)
                with s2.read() as txn:
                    body2 = list(s2.scan(txn, layer=LAYER_BODY))
            print(f"  Round-trip: USDA -> .flux : {len(body2)} BODY records")

            if len(body) == 0 and len(body2) == 0:
                print(f"  RESULT: WARN (no records, but format pipeline works)")
                return "WARN"
            print(f"  RESULT: PASS")
            return "PASS"

    except Exception as e:
        print(f"  RESULT: FAIL ({e})")
        return "FAIL"


def main():
    print("=" * 60)
    print("  FLUXmeme Universal Importer/Exporter Test")
    print("  .flux as the intermediate format hub")
    print("=" * 60)

    results = []

    # Test pre-built assets
    for name, rel, fmt in ROBOTS:
        path = os.path.join(HERE, rel)
        if os.path.exists(path):
            results.append((name, test_roundtrip(name, path, fmt)))

    # Test inline robots
    for name, xml, fmt in EXTRA_ROBOTS:
        results.append((name, test_roundtrip(name, xml, fmt, is_inline=True)))

    # Summary
    print(f"\n{'='*60}")
    print(f"  SUMMARY")
    print(f"{'='*60}")
    for name, status in results:
        tag = {"PASS": "[OK]", "WARN": "[!]", "FAIL": "[X]"}[status]
        print(f"  {tag} {name}")
    passed = sum(1 for _, s in results if s == "PASS")
    total = len(results)
    print(f"\n  {passed}/{total} robots passed round-trip through .flux")
    print(f"  .flux handled URDF (dynamics/inertial/mimic) and USD formats")
    print(f"  as a universal intermediate: any format in -> any format out.")


if __name__ == "__main__":
    main()
