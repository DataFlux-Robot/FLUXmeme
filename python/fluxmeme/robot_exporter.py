"""Robot description exporter — .flux BODY -> URDF / SDF / MJCF XML.

Projects the BODY layer's robot/link + robot/joint records back to standard
robot-description XML formats. Demonstrates .flux as the universal intermediate.

Usage:
    from fluxmeme import Store
    from fluxmeme.robot_exporter import to_urdf, to_sdf, to_mjcf
    with Store("robot.flux") as s, s.read() as txn:
        to_urdf(txn, s, "robot.urdf")
        to_mjcf(txn, s, "robot.xml")
"""
from .api import Store, Record, META_REF
from . import LAYER_BODY


def _collect_robot(store: Store, txn):
    """Scan BODY records; return (links_by_id, joints_by_id)."""
    links = {}  # hex_id -> Record
    joints = {}  # hex_id -> Record
    for r in store.scan(txn, layer=LAYER_BODY):
        if r.kind == "robot/link":
            links[r.id] = r
        elif r.kind == "robot/joint":
            joints[r.id] = r
    return links, joints


def _joint_parent_child(joint):
    """Extract parent_id and child_id from a joint's refs."""
    parent_id = child_id = None
    for rel, target_id, graph in joint.refs:
        if rel == "parent":
            parent_id = target_id
        elif rel == "child":
            child_id = target_id
    return parent_id, child_id


def to_urdf(store: Store, txn, out_path: str) -> int:
    """Export BODY layer to URDF XML. Returns joint count."""
    links, joints = _collect_robot(store, txn)
    lines = ['<?xml version="1.0"?>', '<robot name="fluxmeme_export">']

    for lid, link in links.items():
        name = link.meta.get("name", lid[:8])
        lines.append(f'  <link name="{name}">')
        mass = link.meta.get("mass")
        if mass:
            lines.append(f"    <inertial>")
            lines.append(f'      <mass value="{mass}"/>')
            ixx = link.meta.get("ixx", "0")
            iyy = link.meta.get("iyy", "0")
            izz = link.meta.get("izz", "0")
            lines.append(f'      <inertia ixx="{ixx}" ixy="0" ixz="0" iyy="{iyy}" iyz="0" izz="{izz}"/>')
            lines.append(f"    </inertial>")
        lines.append(f"  </link>")

    count = 0
    for jid, joint in joints.items():
        pid, cid = _joint_parent_child(joint)
        if not pid or not cid:
            continue
        pname = links.get(pid, Record()).meta.get("name", pid[:8])
        cname = links.get(cid, Record()).meta.get("name", cid[:8])
        jname = joint.meta.get("name", f"joint_{count}")
        jtype = joint.meta.get("type", "fixed")
        lines.append(f'  <joint name="{jname}" type="{jtype}">')
        lines.append(f'    <parent link="{pname}"/>')
        lines.append(f'    <child link="{cname}"/>')
        axis = joint.meta.get("axis")
        if axis:
            lines.append(f'    <axis xyz="{axis}"/>')
        lower = joint.meta.get("lower")
        upper = joint.meta.get("upper")
        effort = joint.meta.get("effort", "0")
        velocity = joint.meta.get("velocity", "0")
        if lower is not None and upper is not None:
            lines.append(f'    <limit lower="{lower}" upper="{upper}" effort="{effort}" velocity="{velocity}"/>')
        damping = joint.meta.get("damping")
        friction = joint.meta.get("friction")
        if damping or friction:
            d = damping or "0"
            f = friction or "0"
            lines.append(f'    <dynamics damping="{d}" friction="{f}"/>')
        lines.append(f"  </joint>")
        count += 1

    lines.append("</robot>")
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return count


def to_mjcf(store: Store, txn, out_path: str) -> int:
    """Export BODY layer to MJCF (MuJoCo) XML. Returns joint count."""
    links, joints = _collect_robot(store, txn)
    # Build child->joint map
    child_joints = {}
    for jid, j in joints.items():
        _, cid = _joint_parent_child(j)
        if cid:
            child_joints.setdefault(cid, []).append(j)

    # Find root links (links that are never a child)
    all_children = set()
    for j in joints.values():
        _, cid = _joint_parent_child(j)
        if cid:
            all_children.add(cid)
    roots = [lid for lid in links if lid not in all_children]

    type_map = {"revolute": "hinge", "continuous": "hinge", "prismatic": "slide",
                "fixed": "fixed", "floating": "free", "planar": "slide"}

    lines = ['<?xml version="1.0"?>', '<mujoco model="fluxmeme_export">']
    lines.append('  <worldbody>')

    count = [0]

    def emit_body(lid, indent=2):
        link = links[lid]
        name = link.meta.get("name", lid[:8])
        sp = " " * indent
        lines.append(f'{sp}<body name="{name}">')

        # inertial
        mass = link.meta.get("mass")
        if mass:
            di = f'{link.meta.get("ixx","0")} {link.meta.get("iyy","0")} {link.meta.get("izz","0")}'
            lines.append(f'{sp}  <inertial mass="{mass}" diaginertia="{di}"/>')

        # joints connecting to this body
        for j in child_joints.get(lid, []):
            jname = j.meta.get("name", f"j{count[0]}")
            jtype = type_map.get(j.meta.get("type", "fixed"), "hinge")
            axis = j.meta.get("axis", "0 0 1")
            range_str = ""
            lower = j.meta.get("lower")
            upper = j.meta.get("upper")
            if lower is not None and upper is not None:
                range_str = f' range="{lower} {upper}"'
            lines.append(f'{sp}  <joint name="{jname}" type="{jtype}" axis="{axis}"{range_str}/>')
            count[0] += 1

        # children
        for jid, j in joints.items():
            pid, cid = _joint_parent_child(j)
            if pid == lid and cid in links:
                emit_body(cid, indent + 2)

        lines.append(f"{sp}</body>")

    for rid in roots:
        emit_body(rid)

    lines.append("  </worldbody>")
    lines.append("</mujoco>")
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return count[0]


def to_sdf(store: Store, txn, out_path: str) -> int:
    """Export BODY layer to SDF XML. Returns joint count."""
    links, joints = _collect_robot(store, txn)
    lines = ['<?xml version="1.0"?>', '<sdf version="1.6">', '  <model name="fluxmeme_export">']

    for lid, link in links.items():
        name = link.meta.get("name", lid[:8])
        lines.append(f'    <link name="{name}">')
        mass = link.meta.get("mass")
        if mass:
            lines.append(f"      <inertial>")
            lines.append(f'        <mass>{mass}</mass>')
            lines.append(f'        <inertia>')
            lines.append(f'          <ixx>{link.meta.get("ixx","0")}</ixx>')
            lines.append(f'          <iyy>{link.meta.get("iyy","0")}</iyy>')
            lines.append(f'          <izz>{link.meta.get("izz","0")}</izz>')
            lines.append(f'        </inertia>')
            lines.append(f"      </inertial>")
        lines.append(f"    </link>")

    count = 0
    for jid, joint in joints.items():
        pid, cid = _joint_parent_child(joint)
        if not pid or not cid:
            continue
        pname = links.get(pid, Record()).meta.get("name", pid[:8])
        cname = links.get(cid, Record()).meta.get("name", cid[:8])
        jname = joint.meta.get("name", f"joint_{count}")
        jtype = joint.meta.get("type", "fixed")
        lines.append(f'    <joint name="{jname}" type="{jtype}">')
        lines.append(f'      <parent>{pname}</parent>')
        lines.append(f'      <child>{cname}</child>')
        axis = joint.meta.get("axis")
        if axis:
            lines.append(f'      <axis>')
            lines.append(f'        <xyz>{axis}</xyz>')
            lines.append(f'      </axis>')
        lines.append(f"    </joint>")
        count += 1

    lines.append("  </model>")
    lines.append("</sdf>")
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return count
