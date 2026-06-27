# Example assets

| File | Format | Description |
|---|---|---|
| `cartpole.usda` | USD | Minimal cartpole (cart + pole meshes) for USD <-> .flux demo |
| `quadruped.urdf` | URDF | 4-leg quadruped (9 links, 8 joints with limits/axis) for URDF import |

## Usage

```python
from fluxmeme import Store

# USDA -> .flux
with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.from_usd(txn, "cartpole.usda")

# URDF -> .flux (with dynamics: limits, axis)
with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.from_urdf(txn, "quadruped.urdf")
```

For the **closed-loop 4-bar linkage** (which URDF can't express), see
`demo/demo_robot.c` — it creates links + joints + a closing joint directly in
the .flux graph model.
