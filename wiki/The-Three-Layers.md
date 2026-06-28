# The Three Layers

Every `.flux` holds three natures, each mapping to a standard format. Together,
they make one DevReady asset — not a folder of drifting sidecars.

## BODY — the physical structure

Holds the robot's **physical description**: links, joints, constraints, geometry,
inertial properties, device-comm topology, and affordances.

| `kind` | Description | Key `meta` |
|---|---|---|
| `robot/link` | A rigid body | `name`, `mass` |
| `robot/joint` | An edge (parent->child) | `type` (revolute\|prismatic\|fixed), `lower`, `upper`, `axis` |
| `mesh` | Geometry (binary STL) | — (BIN payload, ptype=`model/stl`) |
| `device-comm/node` | A bus or device | `protocol` (CAN\|RS485\|EtherCAT\|MAVLink), `baud`, `addr` |

**Closed loops** (4-bar, Delta, Stewart) are first-class: the graph model allows
cycles, unlike URDF's pure tree. `flux_robot_has_cycle()` detects them.

Transcodes to: **USD** (`flux_to_usd` — meshes become `def Mesh` prims).

## MIND — the knowledge + agency

Holds what the robot **knows** and **can do**:

| `kind` | Description | Payload |
|---|---|---|
| `concept` | Task knowledge (OKF markdown) | markdown body |
| `agent_card` | An A2A agent card | JSON |
| `task` | A callable skill (linked `part_of` to its card) | JSON |

An LLM/VLA reads MIND directly: `flux_to_okf()` produces a markdown bundle that
an agent ingests for zero-shot task understanding. `flux_to_a2a()` produces an
A2A `agent-card.json` for multi-agent orchestration.

Transcodes to: **OKF** + **A2A**.

## JOURNAL — the lifetime log

An **append-only log** that grows for the robot's entire life:

| `kind` | Description | Key `meta` |
|---|---|---|
| `signal` | A telemetry sample | `name`, `value` |
| `param` | A configuration parameter | `name`, `value` |
| `phm_slice` | A PHM (health management) time slice | timestamp range |

Each record carries a **multi-clock-domain** `clock` field:
`sim_time` / `wall_time` / `device_monotonic` — so sim and real-world telemetry
can be aligned unambiguously.

The journal is what makes `.flux` a **living digital twin**: it grows on-device
(MCU) as the robot operates. `flux_compact` can reclaim growth while preserving
the live state.

Transcodes to: **MCAP** (rosbag2, primary) + **MAVLink** (edge transport; large
assets automatically filtered off the bus).

## Layer routing

Layers are a **bitset** on the `layer` field. A record can belong to one or more
layers. Transcoders and scans filter by layer:

```python
from fluxmeme import Store, LAYER_BODY, LAYER_MIND, LAYER_JOURNAL

with Store("robot.flux") as s, s.read() as txn:
    body_recs    = list(s.scan(txn, layer=LAYER_BODY))      # for USD
    mind_recs    = list(s.scan(txn, layer=LAYER_MIND))      # for OKF + A2A
    journal_recs = list(s.scan(txn, layer=LAYER_JOURNAL))   # for MCAP + MAVLink
```

---

# 三层模型(中文)

每个 `.flux` 承载三种本性,各映射到一个标准格式。三者合一构成 DevReady 资产——
不是一个漂移的 sidecar 文件夹。

## BODY — 物理结构

| `kind` | 说明 | 关键 `meta` |
|---|---|---|
| `robot/link` | 刚体 | `name`, `mass` |
| `robot/joint` | 连接(父→子) | `type`, `lower`, `upper`, `axis` |
| `mesh` | 几何(二进制 STL) | BIN payload |
| `device-comm/node` | 总线/设备 | `protocol`, `baud`, `addr` |

**闭链**(四连杆/Delta/Stewart)是一等公民:graph 模型允许环,超越 URDF 纯树。
投影到:**USD**。

## MIND — 知识 + 能动性

| `kind` | 说明 |
|---|---|
| `concept` | 任务知识(OKF markdown) |
| `agent_card` | A2A agent 卡(JSON) |
| `task` | 可调用技能(链接 `part_of` 到卡) |

LLM/VLA 直接读 MIND:`flux_to_okf()` 产出 markdown 包供 zero-shot 理解。
投影到:**OKF** + **A2A**。

## JOURNAL — 终身日志

| `kind` | 说明 |
|---|---|
| `signal` | 遥测采样 |
| `param` | 配置参数 |
| `phm_slice` | PHM(健康管理)时间切片 |

每条记录带**多时钟域** `clock`:`sim_time` / `wall_time` / `device_monotonic`——
sim 与真实世界遥测可无歧义对齐。日志在设备(MCU)上随运维生长。
投影到:**MCAP**(rosbag2,主)+ **MAVLink**(边缘传输)。
