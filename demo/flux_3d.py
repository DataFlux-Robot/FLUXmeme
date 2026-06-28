#!/usr/bin/env python3
"""FLUXmeme 3D viewer — real mesh rendering with trimesh.

Replaces the matplotlib scatter plot with actual 3D geometry: loads BODY mesh
payloads (STL) and renders them in an interactive scene. For robots without
meshes, renders bounding-box proxies for links.

Usage:
  python demo/flux_3d.py robot.flux              # interactive viewer
  python demo/flux_3d.py robot.flux --save out.png

Requires: pip install trimesh numpy
"""
import sys
import os
import io
import tempfile

def main():
    if len(sys.argv) < 2:
        print("Usage: python flux_3d.py <file.flux> [--save out.png]")
        sys.exit(1)

    flux_path = sys.argv[1]
    save_path = None
    if "--save" in sys.argv:
        idx = sys.argv.index("--save")
        save_path = sys.argv[idx + 1] if idx + 1 < len(sys.argv) else "flux_3d.png"

    try:
        import trimesh
        import numpy as np
    except ImportError:
        print("Install: pip install trimesh numpy")
        sys.exit(1)

    from fluxmeme import Store, LAYER_BODY

    scene_geometries = []

    with Store(flux_path) as s, s.read() as txn:
        records = list(s.scan(txn, layer=LAYER_BODY))

    links = [r for r in records if r.kind == "robot/link"]
    joints = [r for r in records if r.kind == "robot/joint"]
    meshes = [r for r in records if r.kind == "mesh"]

    # ── Load mesh payloads (STL) ──
    for m in meshes:
        if m.payload and len(m.payload) > 84:
            try:
                with tempfile.NamedTemporaryFile(suffix=".stl", delete=False) as tmp:
                    tmp.write(m.payload)
                    tmp_path = tmp.name
                mesh = trimesh.load(tmp_path)
                os.unlink(tmp_path)
                if hasattr(mesh, "geometry"):
                    for g in mesh.geometry.values():
                        scene_geometries.append(g)
                else:
                    scene_geometries.append(mesh)
            except Exception as e:
                print(f"  (skip mesh: {e})")

    # ── If no meshes, create link proxies (boxes) ──
    if not scene_geometries and links:
        n = len(links)
        for i, link in enumerate(links):
            # Place links in a chain layout
            x = (i - n / 2) * 0.3
            box = trimesh.creation.box(extents=[0.15, 0.15, 0.1])
            box.apply_translation([x, 0, 0])
            name = link.meta.get("name", f"link_{i}")
            box.metadata["name"] = name
            # Color by presence of inertia
            has_inertia = "mass" in link.meta
            color = [100, 200, 255, 200] if has_inertia else [255, 180, 100, 200]
            box.visual.face_colors[:] = color
            scene_geometries.append(box)

    if not scene_geometries:
        print("No geometry to render (no meshes, no links with geometry).")
        # Still show joint info
        print(f"\n  Links: {len(links)}, Joints: {len(joints)}, Meshes: {len(meshes)}")
        sys.exit(0)

    # ── Build scene ──
    scene = trimesh.Scene(scene_geometries)
    scene.show_line = True  # axes

    print(f"  FLUXmeme 3D: {flux_path}")
    print(f"  Geometries: {len(scene_geometries)}")
    print(f"  Links: {len(links)}, Joints: {len(joints)}, Meshes: {len(meshes)}")

    if save_path:
        try:
            png = scene.save_image(resolution=[800, 600])
            with open(save_path, "wb") as f:
                f.write(png)
            print(f"  Saved: {save_path}")
        except Exception:
            # Fallback: save as GLB
            glb_path = save_path.replace(".png", ".glb")
            glb = scene.export(file_type="glb")
            with open(glb_path, "wb") as f:
                f.write(glb)
            print(f"  Saved: {glb_path} (GLB format)")
    else:
        scene.show()


if __name__ == "__main__":
    main()
