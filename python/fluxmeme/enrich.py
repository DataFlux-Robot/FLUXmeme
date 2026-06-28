"""FLUXmeme hardware enrichment — extract + attach motor/dynamics/inertial/safety
metadata from URDF XML and motor libraries, matching URDF-Studio's feature set.

This is the application layer: the C engine stores records (dumb); this module
enriches them with structured hardware knowledge.
"""
import json
import os
import xml.etree.ElementTree as ET
from typing import Optional
from . import Store, Record, LAYER_BODY, LAYER_MIND


def enrich_from_urdf(store: Store, urdf_path: str) -> int:
    """Parse a URDF XML and enrich the store's robot/link + robot/joint records
    with inertial (mass + inertia tensor), dynamics (damping/friction),
    mimic joints, and safety controllers. Returns the number of records enriched."""
    tree = ET.parse(urdf_path)
    root = tree.getroot()

    enriched = 0
    link_meta = {}   # name -> {mass, ixx, ...}
    joint_meta = {}  # name -> {damping, friction, mimic_*, safety_*}

    # ── Extract from XML ──
    for link in root.findall("link"):
        name = link.get("name", "")
        inertial = link.find("inertial")
        if inertial is not None:
            mass_el = inertial.find("mass")
            inertia_el = inertial.find("inertia")
            m = {}
            if mass_el is not None:
                m["mass"] = mass_el.get("value", "")
            if inertia_el is not None:
                for attr in ["ixx", "ixy", "ixz", "iyy", "iyz", "izz"]:
                    v = inertia_el.get(attr)
                    if v:
                        m[attr] = v
            if m:
                link_meta[name] = m

    for joint in root.findall("joint"):
        name = joint.get("name", "")
        m = {}
        # dynamics
        dyn = joint.find("dynamics")
        if dyn is not None:
            for attr in ["damping", "friction", "stiffness"]:
                v = dyn.get(attr)
                if v:
                    m[attr] = v
        # mimic
        mimic = joint.find("mimic")
        if mimic is not None:
            for attr in ["joint", "multiplier", "offset"]:
                v = mimic.get(attr)
                if v:
                    m[f"mimic_{attr}"] = v
        # safety controller
        safety = joint.find("safety_controller")
        if safety is not None:
            for attr in ["soft_lower_limit", "soft_upper_limit", "k_position", "k_velocity"]:
                v = safety.get(attr)
                if v:
                    m[attr] = v
        if m:
            joint_meta[name] = m

    # ── Apply to .flux records ──
    with store.read() as txn:
        records = list(store.scan(txn, layer=LAYER_BODY))

    # Build name -> record map (from meta "name")
    by_name = {}
    for r in records:
        nm = r.meta.get("name")
        if nm:
            by_name[nm] = r

    with store.write() as txn:
        for r in records:
            nm = r.meta.get("name", "")
            extra = {}
            if r.kind == "robot/link" and nm in link_meta:
                extra = link_meta[nm]
            elif r.kind == "robot/joint" and nm in joint_meta:
                extra = joint_meta[nm]
            if extra:
                merged = {**r.meta, **extra}
                updated = Record(
                    layer=r.layer, kind=r.kind, id=r.id,
                    meta=merged, payload=r.payload,
                    ptype=r.ptype, path=r.path,
                    links=r.links,
                )
                store.put(txn, updated)
                enriched += 1

    return enriched


def attach_motor(store: Store, joint_name: str, brand: str, motor_type: str,
                 motor_spec: dict, motor_id: str = "", direction: int = 1,
                 gear_ratio: float = 1.0, hw_interface: str = "effort") -> bool:
    """Attach motor/hardware metadata to a named robot/joint record.

    Args:
        motor_spec: dict with keys armature, velocity, effort (from motor library)
    Returns True if the joint was found and updated.
    """
    with store.read() as txn:
        records = list(store.scan(txn, layer=LAYER_BODY))

    target = None
    for r in records:
        if r.kind == "robot/joint" and r.meta.get("name") == joint_name:
            target = r
            break
    if not target:
        return False

    merged = {**target.meta,
        "motor_brand": brand,
        "motor_type": motor_type,
        "motor_armature": str(motor_spec.get("armature", 0)),
        "motor_velocity": str(motor_spec.get("velocity", 0)),
        "motor_effort": str(motor_spec.get("effort", 0)),
        "motor_id": motor_id,
        "motor_direction": str(direction),
        "gear_ratio": str(gear_ratio),
        "hardware_interface": hw_interface,
    }

    with store.write() as txn:
        updated = Record(
            layer=target.layer, kind=target.kind, id=target.id,
            meta=merged, payload=target.payload,
            ptype=target.ptype, links=target.links,
        )
        store.put(txn, updated)
    return True


def load_motor_library(path: str) -> dict:
    """Load a motor library JSON file. Returns {brand: {motor_name: spec_dict}}."""
    with open(path) as f:
        data = json.load(f)
    return data.get("motors", data)


def embed_motor_library(store: Store, lib_path: str):
    """Store a motor library JSON as a MIND-layer record inside the .flux,
    so the catalog travels WITH the asset (unlike URDF-Studio where it's
    a workspace tool resource)."""
    with open(lib_path) as f:
        content = f.read()
    with store.write() as txn:
        store.put(txn, Record(
            layer=LAYER_MIND, kind="motor_library",
            meta={"type": "motor_library", "source": os.path.basename(lib_path)},
            payload=content.encode(),
        ))


def get_motor_library(store: Store) -> Optional[dict]:
    """Retrieve the embedded motor library from the .flux (if present)."""
    with store.read() as txn:
        for r in store.scan(txn, layer=LAYER_MIND):
            if r.kind == "motor_library":
                return json.loads(r.payload)
    return None
