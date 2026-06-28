"""FLUXmeme AI Review — automated robot design validation.

Rule-based scoring engine (no external LLM needed) that evaluates a .flux robot
on multiple dimensions: kinematics, link structure, joint config, collision,
motor selection. Outputs a structured report with scores + issues + suggestions.

Inspired by URDF-Studio's AI Review feature.
"""
import json
from dataclasses import dataclass, field
from typing import List
from . import Store, LAYER_BODY, LAYER_MIND


@dataclass
class Issue:
    severity: str   # "error" | "warning" | "suggestion"
    dimension: str
    message: str
    suggestion: str = ""


@dataclass
class ReviewResult:
    scores: dict = field(default_factory=dict)  # dimension -> 0-100
    issues: List[Issue] = field(default_factory=list)
    overall: int = 0

    def summary(self) -> str:
        lines = [f"{'='*50}", f"  FLUXmeme Design Review", f"{'='*50}"]
        lines.append(f"  Overall: {self.overall}/100")
        lines.append("")
        for dim, score in sorted(self.scores.items(), key=lambda x: x[1]):
            bar = "#" * (score // 5)
            lines.append(f"  {dim:20s} {score:3d}/100  {bar}")
        lines.append("")
        errs = [i for i in self.issues if i.severity == "error"]
        warns = [i for i in self.issues if i.severity == "warning"]
        suggs = [i for i in self.issues if i.severity == "suggestion"]
        lines.append(f"  Errors: {len(errs)}  Warnings: {len(warns)}  Suggestions: {len(suggs)}")
        for i in self.issues:
            tag = {"error": "ERR", "warning": "WRN", "suggestion": "SUG"}[i.severity]
            lines.append(f"  [{tag}] ({i.dimension}) {i.message}")
            if i.suggestion:
                lines.append(f"        -> {i.suggestion}")
        lines.append(f"{'='*50}")
        return "\n".join(lines)

    def to_json(self) -> str:
        return json.dumps({
            "overall": self.overall,
            "scores": self.scores,
            "issues": [{"severity": i.severity, "dimension": i.dimension,
                         "message": i.message, "suggestion": i.suggestion}
                        for i in self.issues],
        }, indent=2, ensure_ascii=False)


def review_robot(store: Store) -> ReviewResult:
    """Run a full design review on the .flux robot. Returns a ReviewResult."""
    result = ReviewResult()

    with store.read() as txn:
        records = list(store.scan(txn, layer=LAYER_BODY))

    links = [r for r in records if r.kind == "robot/link"]
    joints = [r for r in records if r.kind == "robot/joint"]
    meshes = [r for r in records if r.kind == "mesh"]

    # ── 1. Kinematics Design ──
    score = 100
    n_dof = sum(1 for j in joints if j.meta.get("type") in
                ("revolute", "prismatic", "continuous"))
    if n_dof == 0:
        result.issues.append(Issue("error", "Kinematics", "No actuated joints (0 DoF)"))
        score = 0
    elif n_dof < 3:
        result.issues.append(Issue("warning", "Kinematics", f"Only {n_dof} DoF — limited mobility"))
        score -= 20
    if len(links) < 2:
        result.issues.append(Issue("error", "Kinematics", "Fewer than 2 links — not a robot"))
        score -= 50
    # Check for closed loops (a strength of .flux)
    has_cycle = any(True for j in joints if "loop" in j.meta.get("name", "").lower())
    result.scores["Kinematics"] = max(0, score)

    # ── 2. Link Structure ──
    score = 100
    links_with_inertia = sum(1 for l in links if "mass" in l.meta or "ixx" in l.meta)
    if links and links_with_inertia == 0:
        result.issues.append(Issue("error", "Link Structure",
            "No inertial properties on any link — physics simulation will fail",
            "Add <inertial><mass/><inertia/></inertial> to URDF links"))
        score = 20
    elif links and links_with_inertia < len(links):
        missing = len(links) - links_with_inertia
        result.issues.append(Issue("warning", "Link Structure",
            f"{missing}/{len(links)} links missing inertial properties"))
        score -= missing * 10
    if not meshes:
        result.issues.append(Issue("suggestion", "Link Structure",
            "No mesh geometry — visualization will be empty",
            "Add visual/collision meshes via USD import or STL payload"))
    result.scores["Link Structure"] = max(0, score)

    # ── 3. Joint Configuration ──
    score = 100
    for j in joints:
        jtype = j.meta.get("type", "fixed")
        if jtype in ("revolute", "prismatic"):
            if "lower" not in j.meta or "upper" not in j.meta:
                result.issues.append(Issue("warning", "Joint Config",
                    f"Joint '{j.meta.get('name','?')}' ({jtype}) has no limits",
                    "Add <limit lower/upper/> to the joint"))
                score -= 10
            if "effort" not in j.meta:
                result.issues.append(Issue("suggestion", "Joint Config",
                    f"Joint '{j.meta.get('name','?')}' has no max effort"))
                score -= 5
    # Check dynamics
    joints_with_damping = sum(1 for j in joints if "damping" in j.meta)
    if joints and joints_with_damping == 0:
        result.issues.append(Issue("warning", "Joint Config",
            "No joint dynamics (damping/friction) — sim may be unstable",
            "Add <dynamics damping=... friction=.../>"))
        score -= 15
    result.scores["Joint Config"] = max(0, score)

    # ── 4. Motor Selection ──
    score = 100
    joints_with_motor = sum(1 for j in joints if "motor_type" in j.meta)
    if not joints:
        score = 0
    elif joints_with_motor == 0:
        result.issues.append(Issue("warning", "Motor Selection",
            "No motor metadata on any joint — hardware specs unknown",
            "Attach motors from motor_library.json"))
        score = 30
    else:
        # Check effort vs motor max effort
        for j in joints:
            if "motor_effort" in j.meta and "effort" in j.meta:
                joint_effort = float(j.meta.get("effort", 0))
                motor_effort = float(j.meta.get("motor_effort", 0))
                if motor_effort > 0 and joint_effort > motor_effort * 1.5:
                    result.issues.append(Issue("error", "Motor Selection",
                        f"Joint '{j.meta.get('name','?')}' requires {joint_effort} Nm "
                        f"but motor '{j.meta.get('motor_type','?')}' max is {motor_effort} Nm",
                        f"Use a stronger motor (>= {joint_effort*1.2:.0f} Nm)"))
                    score -= 20
        coverage = joints_with_motor / max(1, len(joints))
        if coverage < 1.0:
            result.issues.append(Issue("suggestion", "Motor Selection",
                f"Motor coverage: {joints_with_motor}/{len(joints)} joints"))
            score -= int((1 - coverage) * 30)
    result.scores["Motor Selection"] = max(0, score)

    # ── 5. Collision Detection ──
    score = 100
    if not meshes:
        result.issues.append(Issue("warning", "Collision",
            "No mesh geometry — cannot do collision detection"))
        score = 0
    else:
        result.issues.append(Issue("suggestion", "Collision",
            f"{len(meshes)} mesh(es) found — verify collision geometry coverage"))
    result.scores["Collision"] = score

    # ── Overall (weighted) ──
    weights = {"Kinematics": 0.25, "Link Structure": 0.20, "Joint Config": 0.20,
               "Motor Selection": 0.20, "Collision": 0.15}
    result.overall = int(sum(result.scores.get(k, 0) * w for k, w in weights.items()))

    return result
