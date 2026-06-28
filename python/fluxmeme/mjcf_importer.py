"""MJCF (MuJoCo XML) importer — MJCF -> .flux BODY records.

Parses <body name=> + <joint name= type=> nesting into robot/link + robot/joint
records with REF connections (@mechanical graph). Handles the MuJoCo body-tree
structure (parent = enclosing body).

Usage:
    from fluxmeme import Store
    from fluxmeme.mjcf_importer import from_mjcf
    with Store("robot.flux", writable=True) as s, s.write() as txn:
        from_mjcf(txn, "robot.xml")
"""
import xml.etree.ElementTree as ET
from .api import Store, Record, META_REF, META_FLOAT, META_STRING
from . import LAYER_BODY


def _parse_vec3(s):
    """Parse "1.0 2.0 3.0" -> (1.0, 2.0, 3.0)"""
    try:
        parts = s.strip().split()
        return tuple(float(p) for p in parts[:3])
    except (ValueError, IndexError):
        return (0.0, 0.0, 0.0)


def _add_ref(meta_list, meta_types_list, refs_list, key, target_id, graph="mechanical"):
    """Add a REF connection."""
    refs_list.append((key, target_id, graph))


def from_mjcf(store: Store, txn, mjcf_path: str) -> int:
    """Import a MuJoCo MJCF XML file into the store. Returns number of records."""
    tree = ET.parse(mjcf_path)
    root = tree.getroot()

    # Find worldbody (the root of the body tree)
    worldbody = root.find(".//worldbody")
    if worldbody is None:
        return 0

    count = 0
    # Stack-based traversal: (element, parent_id_or_None)
    stack = [(worldbody, None)]
    while stack:
        elem, parent_id = stack.pop()

        for body in elem.findall("body"):
            name = body.get("name", f"body_{count}")
            # Create robot/link record
            link_rec = Record(layer=LAYER_BODY, kind="robot/link", meta={"name": name})

            # Parse inertial if present
            inertial = body.find("inertial")
            if inertial is not None:
                mass = inertial.get("mass")
                if mass:
                    link_rec.meta["mass"] = mass
                    link_rec.meta_types = getattr(link_rec, "meta_types", {})
                    link_rec.meta_types["mass"] = META_FLOAT
                # MuJoCo uses diaginertia or fullinertia
                diaginertia = inertial.get("diaginertia")
                if diaginertia:
                    parts = diaginertia.strip().split()
                    for idx, key in enumerate(["ixx", "iyy", "izz"]):
                        if idx < len(parts):
                            link_rec.meta[key] = parts[idx]

            store.put(txn, link_rec)
            count += 1

            # Parse joints inside this body
            for joint in body.findall("joint"):
                jname = joint.get("name", f"joint_{count}")
                jtype = joint.get("type", "hinge")
                # Map MJCF types to URDF-like
                type_map = {"hinge": "revolute", "slide": "prismatic", "ball": "continuous",
                           "free": "floating", "fixed": "fixed"}
                mapped_type = type_map.get(jtype, jtype)

                jmeta = {"name": jname, "type": mapped_type}
                jmeta_types = {"name": META_STRING, "type": META_STRING}

                # axis
                axis = joint.get("axis")
                if axis:
                    jmeta["axis"] = axis

                # range -> limits
                rng = joint.get("range")
                if rng:
                    parts = rng.strip().split()
                    if len(parts) >= 2:
                        jmeta["lower"] = parts[0]
                        jmeta["upper"] = parts[1]
                        jmeta_types["lower"] = META_FLOAT
                        jmeta_types["upper"] = META_FLOAT

                # actuated force
                actfrc = joint.get("actuatorfrcrange")
                if actfrc:
                    parts = actfrc.strip().split()
                    if len(parts) >= 1:
                        jmeta["effort"] = str(abs(float(parts[1])) if len(parts) > 1 else float(parts[0]))
                        jmeta_types["effort"] = META_FLOAT

                # damping/friction
                damping = joint.get("damping")
                if damping:
                    jmeta["damping"] = damping
                    jmeta_types["damping"] = META_FLOAT
                frictionloss = joint.get("frictionloss")
                if frictionloss:
                    jmeta["friction"] = frictionloss
                    jmeta_types["friction"] = META_FLOAT

                joint_rec = Record(
                    layer=LAYER_BODY, kind="robot/joint",
                    meta=jmeta, meta_types=jmeta_types,
                )
                # Connect joint to its parent body (enclosing) and this body
                if parent_id is not None:
                    joint_rec.refs = [("parent", parent_id, "mechanical")]
                joint_rec.refs.append(("child", link_rec.id, "mechanical"))

                store.put(txn, joint_rec)
                count += 1

            # Push children onto stack with this body as parent
            stack.append((body, link_rec.id))

    return count
