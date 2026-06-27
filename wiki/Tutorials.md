# Tutorials

Step-by-step guides for common tasks. Each is self-contained — copy and run.

## Tutorial 1: Build a robot from scratch

Create a 2-link robot with a revolute joint, entirely in Python:

```python
from fluxmeme import Store, Record, LAYER_BODY

with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        # Link 1: base
        r1 = Record(layer=LAYER_BODY, kind="robot/link", meta={"name": "base"})
        s.put(txn, r1)

        # Link 2: arm
        r2 = Record(layer=LAYER_BODY, kind="robot/link", meta={"name": "arm"})
        s.put(txn, r2)

        # Joint: base -> arm (revolute, with limits + axis)
        r3 = Record(
            layer=LAYER_BODY, kind="robot/joint",
            meta={"type": "revolute", "lower": "-1.57", "upper": "1.57",
                  "axis": "0 0 1", "effort": "10"},
            links=[(r2.id, "parent"), (r1.id, "child")],  # parent=arm? -> check
        )
        # Actually: parent=base, child=arm. Links are (target_hex, rel).
        # base is parent of arm: link[0] = (base_id, "parent"), link[1] = (arm_id, "child")
        r3.links = [(r1.id, "parent"), (r2.id, "child")]
        s.put(txn, r3)

    print(f"Created robot.flux with 2 links + 1 joint")
```

Verify with the CLI:
```bash
flux dump robot.flux
```

## Tutorial 2: Add knowledge (MIND layer)

Turn a physical robot into a **DevReady** asset by adding task knowledge:

```python
from fluxmeme import Store, Record, LAYER_MIND

with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        # Task concept (OKF markdown)
        s.put(txn, Record(
            layer=LAYER_MIND, kind="concept",
            meta={"title": "Reach and grasp",
                  "tags": "manipulation,grasping",
                  "type": "concept"},
            payload=b"""# Reach and Grasp Task

## Objective
Reach the target object and grasp it.

## Steps
1. Move end-effector to pre-grasp position
2. Close gripper
3. Lift object
4. Move to placement target
5. Release

## Success criteria
- Object is at the target position
- No collisions during motion
""",
        ))

        # Agent card (A2A)
        s.put(txn, Record(
            layer=LAYER_MIND, kind="agent_card",
            ptype="application/json",
            payload=b'{"name":"grasp_agent","version":"1.0","skills":["reach","grasp","place"]}',
        ))

    # Project MIND to OKF (markdown bundle)
    with s.read() as txn:
        s.to_okf(txn, "knowledge/")
    print("Exported knowledge/ — an LLM can read this for zero-shot task understanding")
```

## Tutorial 3: USD round-trip

Import a USDA, modify in .flux, export back:

```python
from fluxmeme import Store, Record, LAYER_BODY, LAYER_MIND

# 1. Import USD -> .flux
with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.from_usd(txn, "demo/assets/cartpole.usda")

    # 2. Add knowledge (USD can't carry this)
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                          meta={"title": "Balance the pole"},
                          payload=b"# Keep the pole upright."))

    # 3. Export back -> USD (BODY only; MIND stays in .flux)
    with s.read() as txn:
        s.to_usd(txn, "robot_from_flux.usda")

    # 4. Show the advantage
    with s.read() as txn:
        body = list(s.scan(txn, layer=LAYER_BODY))
        mind = list(s.scan(txn, layer=LAYER_MIND))
        print(f".flux: {len(body)} BODY + {len(mind)} MIND")
        print(f"USD:  {len(body)} BODY only (MIND lost)")
```

## Tutorial 4: Composition (LIVRPS override)

Override a robot parameter non-destructively:

```python
from fluxmeme import Store, Record, LAYER_BODY

# Base layer
with Store("base.flux", writable=True) as s:
    with s.write() as txn:
        r = Record(layer=LAYER_BODY, kind="robot/link",
                   meta={"name": "base", "mass": "10", "color": "red"})
        s.put(txn, r)
        base_id = r.id

# Override layer (same id, different mass)
with Store("override.flux", writable=True) as s:
    with s.write() as txn:
        r = Record(layer=LAYER_BODY, kind="robot/link", meta={"mass": "20"})
        r.id = base_id
        s.put(txn, r)

# Root declares the stack
with Store("root.flux", writable=True) as s:
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_MIND, kind="flux/compose",
                          meta={"sublayers": "override.flux;base.flux"}))
```

Resolve:
```bash
flux compose root.flux
# Merged: mass=20 (override wins), color=red (base fills gap), name=base
```

## Tutorial 5: Lifecycle (generate -> reuse -> operate)

```python
from fluxmeme import Store, Record, LAYER_BODY, LAYER_MIND, LAYER_JOURNAL

# 1. GENERATE
with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.from_urdf(txn, "demo/assets/quadruped.urdf")  # body
        s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                          meta={"title": "Walk forward"},
                          payload=b"# Walk at 0.5 m/s."))  # mind

# 2. REUSE (project to tools)
with Store("robot.flux") as s, s.read() as txn:
    s.to_usd(txn, "for_sim.usda")       # Newton / Isaac Sim
    s.to_okf(txn, "for_vla/")           # LLM task prompting

# 3. OPERATE (journal grows)
with Store("robot.flux", writable=True) as s:
    for step in range(10):
        with s.write() as txn:
            s.put(txn, Record(layer=LAYER_JOURNAL, kind="signal",
                              meta={"name": f"joint_{step}", "value": "0.1"}))

# 4. REPLAY
with Store("robot.flux") as s, s.read() as txn:
    journal = list(s.scan(txn, layer=LAYER_JOURNAL))
    s.to_mcap(txn, "run.mcap")
    print(f"Journal: {len(journal)} signals -> MCAP exported")
```

One file, born in reality, perpetually real.

## Tutorial 6: Visualize a .flux

```bash
pip install matplotlib numpy
python demo/flux_viz.py robot.flux --save viz.png
```

Renders: record graph (nodes + edges), layer breakdown bar chart, kind distribution.

---

# 教程(中文)

逐步指南,每个都自包含——复制即跑。

## 教程 1:从零构建机器人

```python
from fluxmeme import Store, Record, LAYER_BODY

with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        r1 = Record(layer=LAYER_BODY, kind="robot/link", meta={"name": "base"})
        s.put(txn, r1)
        r2 = Record(layer=LAYER_BODY, kind="robot/link", meta={"name": "arm"})
        s.put(txn, r2)
        r3 = Record(layer=LAYER_BODY, kind="robot/joint",
                    meta={"type": "revolute", "lower": "-1.57", "upper": "1.57", "axis": "0 0 1"},
                    links=[(r1.id, "parent"), (r2.id, "child")])
        s.put(txn, r3)
    print("创建了 robot.flux: 2 links + 1 joint")
```

## 教程 2:添加知识(MIND 层)

```python
from fluxmeme import Store, Record, LAYER_MIND

with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                          meta={"title": "抓取放置"},
                          payload=b"# 抓取物体,放到目标。\n## 步骤\n1. 移动到预抓位\n2. 闭合夹爪\n3. 抬起"))
    with s.read() as txn:
        s.to_okf(txn, "knowledge/")
    print("导出 knowledge/ — LLM 可直接读用于 zero-shot 任务理解")
```

## 教程 3:USD 往返

```python
from fluxmeme import Store, Record, LAYER_BODY, LAYER_MIND

with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.from_usd(txn, "demo/assets/cartpole.usda")  # 导入
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_MIND, kind="concept",  # 加知识
                          meta={"title": "平衡杆"}, payload=b"# 保持杆竖直。"))
    with s.read() as txn:
        s.to_usd(txn, "robot_from_flux.usda")  # 导出
        body = list(s.scan(txn, layer=LAYER_BODY))
        mind = list(s.scan(txn, layer=LAYER_MIND))
        print(f".flux: {len(body)} BODY + {len(mind)} MIND(USD 仅 {len(body)} BODY)")
```

## 教程 4:组合(LIVRPS 覆盖)

```bash
flux compose root.flux
# 合并: mass=20(覆盖层赢), color=red(基础层补缺)
```

## 教程 5:生命周期

```python
# 生成 -> 复用 -> 运维,同一个 .flux
# 见上方英文版 Tutorial 5(代码通用)
```

## 教程 6:可视化

```bash
pip install matplotlib numpy
python demo/flux_viz.py robot.flux --save viz.png
```
