<div align="center">

# FLUXmeme

### **DevReady assets — the upgrade from SimReady**

**One robot. One `.flux` file. Body + mind + lifetime journal.**
**一个机器人。一个 `.flux`。身体 + 心智 + 终身日志。**

The self-describing asset format for embodied AI — readable & writable from the
cloud down to a Cortex-M7 MCU, byte-for-byte identical.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Language: C11](https://img.shields.io/badge/language-C11-blue.svg)]()
[![CI](https://img.shields.io/badge/CI-Win%20%2B%20Linux-green.svg)](.github/workflows/ci.yml)
[![Spec](https://img.shields.io/badge/spec-v1.0--draft-orange.svg)](SPEC.md)
[![Python](https://img.shields.io/badge/python-ctypes-blue.svg)](python/)
[![Status](https://img.shields.io/badge/status-v0.1%20reference-red.svg)]()

</div>

> **FLUXmeme is the format for *Development-Ready (DevReady)* assets** —
> Lightwheel's SimReady, upgraded. SimReady anchors a 3D asset to real physics
> (USD / the *world*). **DevReady adds the *mind*, the *agent*, the *lifetime
> journal*, and a path to the *edge*** — all in one self-describing artifact.

---

# English

## The shift: SimReady → DevReady

NVIDIA's **SimReady** made 3D assets *physically accurate* — a mesh carries
measured mass, friction, deformation. That solved **scene fidelity** for sim.
But a modern robot is not a scene. It is an **embodied node** that also has
**knowledge** (what to do), **agency** (skills it can call), and a **lifetime
health record** (what happened to it). Those live today in 5+ drifting sidecar
files no single tool understands — and none run on the robot itself.

**DevReady unifies them into one `.flux`:**

| | **SimReady** (USD/world) | **DevReady** (FLUXmeme `.flux`) |
|---|---|---|
| **What it carries** | scene + measured physics | scene + **knowledge + agent + lifetime journal** |
| **Asset boundary** | many files (USD + URDF + docs + JSON + MCAP + YAML) | **one self-describing file** |
| **Agent-native** | no (needs external runtime to interpret) | **yes** — `flux inspect`; any agent reads/writes directly |
| **Composition** | USD layers (scene-centric) | **LIVRPS, decentralized** (each robot = one autonomous node) |
| **Robot model** | tree (URDF); closed loops need sidecars | **graph, closed loops first-class** (4-bar / Delta / Stewart) |
| **Edge / MCU** | no (USD is desktop-only) | **yes** — lean C core runs on a Cortex-M7; same bytes |
| **Lifecycle** | born in sim, dies in sim | **generate → reuse → operate** — the journal grows on-device |

The design bet: **the asset that accompanies an embodied agent through its entire
life should be one artifact** — not a folder that drifts apart. SimReady made
assets physically real; DevReady makes them *cognitively and operationally*
real, and *edge-portable*.

## Design philosophy

1. **One artifact through the whole lifecycle.** A robot is *generated* (design
   → graph body), *reused* (sim, deployment, agent collaboration), then
   *operated* (the same file accrues its PHM health journal on-device). One
   `.flux` is the digital thread — no reconciliation between sidecars.
2. **Dumb storage, smart application.** The format stores only TEXT/BIN payloads
   + structured metadata. Parsing, rendering, FK/IK solving, scheduling — all
   stay in the application layer. Adding a new domain is a new `kind`, never an
   engine change. This is what keeps the core small enough for an MCU.
3. **Decentralized by construction.** Each `.flux` is one self-rooted, autonomous
   embodied node — no global scene root. A multi-robot world is a peer
   composition of nodes, organized by upper layers.

## What lives in a `.flux`

A `.flux` is **one embodied node** with three natures (`layer`):

| `layer` | What it holds | Transcodes to |
|---|---|---|
| **BODY** | graph kinematics (**closed-loop**, beyond URDF), geometry/inertia, **affordance**, device-comm topology | → **USD** |
| **MIND** | task knowledge (OKF markdown), agent skills/cards (A2A) | → **OKF + A2A** |
| **JOURNAL** | telemetry/params, **PHM black-box** that grows for the device's life; multi-clock-domain (`sim_time`/`wall_time`/`device_monotonic`) | → **MCAP** + **MAVLink** |

**One source, many projections.** Transcoders are *views* — FLUXmeme doesn't
"contain a USD file," it *is* the asset and renders a USD (or OKF, A2A, MCAP,
MAVLink) view on demand. Your existing tools keep working; FLUXmeme is the
single source behind them.

## Capabilities (reference implementation, v0.1)

- **5 transcoders, all round-trip tested:** USD ↔ BODY (STL↔Mesh), OKF ↔ MIND,
  A2A ↔ MIND, MCAP ↔ JOURNAL, MAVLink ↔ JOURNAL (large assets filtered off the bus).
- **Full-platform, byte-identical:** one pure-C core; desktop (Win/Mac/Linux) +
  **MCU** (designed-for STM32H7/FLUXLOOP, ESP32, multi-RTOS: FreeRTOS/Zephyr/
  ThreadX/RT-Thread). `.flux` written on a PC is read unchanged on an MCU.
- **LIVRPS composition:** layer stacks + **field-level merge** + **variants**,
  non-destructive; any node can be a composition root.
- **Closed-loop robot graphs** (cycle detection), **device-comm topology** with
  per-protocol routing; **URDF/SDF import**; **SimReady→DevReady** one-command skill.
- **Concurrency:** append-only log + MVCC (lock-free readers, single writer),
  optimistic CAS, FIFO cursars, `flux compact`.
- **Dual form:** `.fluxa` (human-readable, diff-friendly **canonical source**) ↔
  `.flux` (binary) via `flux conv`. GitHub-native.
- **Self-describing:** `flux inspect` identifies any file; agents read/write
  **without a runtime**.
- **Tooling:** `flux` CLI (inspect/dump/transcode/conv/compose/from-simready),
  Python bindings (validated, ctypes).
- **Engineering baseline:** untrusted-input hardening caps, CI matrix
  (Win MSVC + Linux gcc), governance (CONTRIBUTING/COC/RFC/SECURITY).

## Quickstart

```bat
:: Windows (VS 2022 Developer prompt, or the VS-bundled CMake):
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\bin\Release\demo_lifecycle.exe   :: generate -> reuse -> operate, one .flux
```
```bash
# Linux / macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/bin/demo_lifecycle
```

```c
#include "fluxmeme/fluxmeme.h"
flux_store_t* s;  flux_open("robot.flux", 1, &s);
flux_txn_t* w;  flux_txn_begin_write(s, &w);
flux_record_t r = {0};  r.layer = FLUX_LAYER_BODY;  r.kind = "robot/link";
flux_meta_kv_t m = {"name","base"};  r.meta=&m;  r.meta_count=1;
flux_put(w, &r);  flux_txn_commit(w);

flux_txn_t* rd;  flux_txn_begin_read(s, &rd);
flux_to_usd(rd, "scene.usda");   /* BODY view */
flux_to_okf(rd, "okf_out/");     /* MIND view */
```
```python
from fluxmeme import Store, Record, LAYER_MIND
with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_MIND, kind="concept", payload=b"# Greet"))
    with s.read() as txn:
        s.to_okf(txn, "okf_out/")
```

## Architecture (three tiers)

```
[Tier 3] platform SDKs   Python (ctypes) | `flux` CLI | … Rust/JS/ROS2 later
[Tier 2] C middleware     port/HAL + core + codec + compose   (one codebase, MCU-budget)
[Tier 1] identical format .flux (binary) / .fluxa (text)       (byte-identical)
```
The **format is invariant**; platform differences are absorbed by Tier 2/3. The
Tier-2 budget is *"runs on a Cortex-M7"* — that constraint is the razor keeping
the core honest, lean, and universal (heavy work — OpenUSD, URDF parsing, Python,
full LIVRPS — is Tier 3 and never compiled into MCU firmware). See
[ARCHITECTURE.md](docs/ARCHITECTURE.md) and [SPEC.md](SPEC.md).

## Roadmap

- ✅ **v0.1** core engine + 5 transcoders + LIVRPS composition + robot/device
  facets + URDF/SDF import + Python + CLI + governance/CI/hardening (this repo)
- 🚧 **v1.0** tag; finalize SPEC; richer robot dynamics (inertia/actuator)
- 🔜 **v1.1+** MJCF import; inherit/specialize arcs; **CAN / EtherCAT** frame codecs
- 🔜 **MCU** STM32H7/FLUXLOOP on-device full read/write + the on-device PHM hero

## For adopters

- **Robotics teams:** replace the drifting sidecar folder with one artifact that
  your robot *and* your simulator *and* your agents share — and that travels to
  the edge. URDF/SDF/SimReady import is one command.
- **Sim platforms (NVIDIA Isaac Sim, Gazebo):** FLUXmeme is the **single source**
  behind your USD/MCAP; you keep your tools, gain a unified, agent-readable,
  lifecycle-spanning asset.
- **Agent / VLA startups:** agents read assets **without a runtime**; one `.flux`
  feeds sim scene + task knowledge + skills + live telemetry.
- **Standards-minded:** MIT, spec-first (independent single-page SPEC), open
  governance from day one, on a path to a foundation (Linux Foundation / ASWF).

**The wedge:** *one self-describing asset = scene + knowledge + agent + signal,
natively composable, agent-readable — a single asset that is one reason-able,
action-able unit.* USD + sidecars can't be that. That's DevReady.

---

# 中文版

## 从 SimReady 到 DevReady 的跃迁

NVIDIA 的 **SimReady** 让 3D 资产**物理准确**——一个 mesh 带着实测的质量、摩擦、形变。它解决了仿真的**场景保真度**。但今天的机器人不是一个场景,它是一个**具身节点**:还要有**知识**(做什么)、**能动性**(可调用的技能)、和一份**终身健康档案**(经历了什么)。这些今天散落在 5+ 个互不相识、各自漂移的 sidecar 文件里——而且**没有一个能在机器人本体上运行**。

**DevReady 把它们统一进一个 `.flux`:**

| | **SimReady**(USD/世界) | **DevReady**(FLUXmeme `.flux`) |
|---|---|---|
| **承载什么** | 场景 + 实测物理 | 场景 + **知识 + agent + 终身日志** |
| **资产边界** | 多文件(USD + URDF + 文档 + JSON + MCAP + YAML) | **一个自描述文件** |
| **agent 原生** | 否(需外部运行时解读) | **是**——`flux inspect`;任何 agent 直接读写 |
| **组合** | USD 层(以场景为中心) | **LIVRPS,去中心化**(每个机器人 = 一个自治节点) |
| **机器人模型** | 纯树(URDF);闭链要 sidecar | **图,闭链一等公民**(四连杆/Delta/Stewart) |
| **边缘 / MCU** | 否(USD 仅桌面) | **是**——精简 C 核心跑在 Cortex-M7;字节相同 |
| **生命周期** | 生于仿真,死于仿真 | **生成→复用→运维**——日志在设备上生长 |

设计赌注:**伴随具身 agent 一生的资产应该是一个工件**——不是一个会漂移分离的文件夹。SimReady 让资产"物理真实";DevReady 让资产"**认知与操作真实**",且**可上边缘**。

## 设计哲学

1. **一个工件贯穿全生命周期。** 机器人被*生成*(设计→graph 身体)、*复用*(仿真/部署/agent 协作)、再*运维*(同一文件在设备上累积 PHM 健康日志)。一个 `.flux` 就是数字主线——sidecar 间无需对账。
2. **哑存储,智应用。** 格式只存 TEXT/BIN 载荷 + 结构化元数据。解析/渲染/FK·IK 求解/调度都留在应用层。加新领域 = 加一个 `kind`,不改引擎。这正是核心小到能进 MCU 的原因。
3. **天然去中心化。** 每个 `.flux` 是一个自根、自治的具身节点——无全局场景根。多机器人世界是节点的对等组合,由上层组织。

## 一个 `.flux` 里有什么

一个 `.flux` 是**一个具身节点**,三种本性(`layer`):

| `layer` | 承载 | 转录到 |
|---|---|---|
| **BODY** | graph 运动学(**闭链**,超越 URDF)+ 几何/惯量 + **affordance** + 设备通信拓扑 | → **USD** |
| **MIND** | 任务知识(OKF markdown)+ agent 技能/卡(A2A) | → **OKF + A2A** |
| **JOURNAL** | 遥测/参数 + **PHM 黑匣子**(随设备寿命生长;多时钟域) | → **MCAP** + **MAVLink** |

**一源多投影。** transcoder 是*视图*——FLUXmeme 不"内含一个 USD",它**就是**这个资产,按需渲染出 USD(或 OKF/A2A/MCAP/MAVLink)视图。你的现有工具照常工作;FLUXmeme 是背后的单一源。

## 能力(参考实现 v0.1)

- **5 个 transcoder,全往返测试**:USD ↔ BODY(STL↔Mesh)、OKF ↔ MIND、A2A ↔ MIND、MCAP ↔ JOURNAL、MAVLink ↔ JOURNAL(大资产自动过滤不上总线)。
- **全平台、字节相同**:一份纯 C 核心;桌面(Win/Mac/Linux)+ **MCU**(为 STM32H7/FLUXLOOP、ESP32、多 RTOS:FreeRTOS/Zephyr/ThreadX/RT-Thread 设计)。PC 上写的 `.flux` 烧到 MCU 上原样可读。
- **LIVRPS 组合**:层栈 + **字段级合并** + **variant**,非破坏;任意节点可作组合根。
- **闭链 robot 图**(环检测)+ **device-comm 拓扑**(按协议路由);**URDF/SDF 导入**;**SimReady→DevReady** 一键 skill。
- **并发**:append-only 日志 + MVCC(无锁多读者、单写者)、乐观 CAS、FIFO cursor、`flux compact`。
- **双形态**:`.fluxa`(人可读、可 diff 的**canonical 源)↔ `.flux`(二进制),`flux conv`。GitHub 原生。
- **自描述**:`flux inspect` 识别任意文件;agent **无需运行时**即可读写。
- **工具**:`flux` CLI(inspect/dump/transcode/conv/compose/from-simready)、Python 绑定(已验证,ctypes)。
- **工程基线**:不可信输入硬化 caps、CI 矩阵(Win MSVC + Linux gcc)、治理(CONTRIBUTING/COC/RFC/SECURITY)。

## 快速开始

见上方 **Quickstart**(英文段)——Windows / Linux / macOS 命令相同;C 与 Python 示例亦通用。

## 三层架构

```
[Tier 3] 各平台 SDK   Python(ctypes)| `flux` CLI | … Rust/JS/ROS2 后续
[Tier 2] C 中间件      port/HAL + 核心 + codec + 组合   (一份代码,MCU 预算)
[Tier 1] 相同格式      .flux(二进制)/ .fluxa(文本)    (字节级一致)
```
**格式不变应万变**;平台差异由 Tier 2/3 吸收。Tier-2 复杂度上限 = **"能在 Cortex-M7 跑"**——这个约束是让核心保持诚实、精简、通用的剃刀(OpenUSD、URDF 解析、Python、完整 LIVRPS 等重物是 Tier 3,永不进 MCU 固件)。详见 [ARCHITECTURE.md](docs/ARCHITECTURE.md)、[SPEC.md](SPEC.md)。

## 路线

- ✅ **v0.1** 核心引擎 + 5 transcoder + LIVRPS 组合 + robot/device facet + URDF/SDF 导入 + Python + CLI + 治理/CI/硬化(本仓库)
- 🚧 **v1.0** 打 tag;SPEC 定稿;更丰富机器人动力学(惯量/actuator)
- 🔜 **v1.1+** MJCF 导入;inherit/specialize 弧;**CAN / EtherCAT** 帧 codec
- 🔜 **MCU** STM32H7/FLUXLOOP 真机全读写 + on-device PHM hero

## 给采用者

- **机器人团队**:用**一个工件**替换漂移的 sidecar 文件夹——你的机器人、仿真器、agent 共享同一份,且能上边缘。URDF/SDF/SimReady 一键导入。
- **仿真平台(NVIDIA Isaac Sim、Gazebo)**:FLUXmeme 是 USD/MCAP 背后的**单一源**——保留你的工具,获得一个统一、agent 可读、贯穿生命周期的资产。
- **agent / VLA 初创**:agent **无需运行时**即可读资产;一个 `.flux` 同时喂仿真场景 + 任务知识 + 技能 + 实时遥测。
- **标准化导向**:MIT、spec 优先(独立单页 SPEC)、day-1 开放治理、瞄 foundation(Linux Foundation / ASWF)。

**致命 wedge:** *一个自描述资产 = 场景 + 知识 + agent + 信号,原生组合,agent 可读——一个资产即一个可推理、可操作的单元。* USD + sidecar 做不到。这就是 DevReady。

---

<div align="center">

**MIT licensed · spec-first · open governance.** Contributions welcome — see
[CONTRIBUTING.md](CONTRIBUTING.md). Read the [SPEC](SPEC.md) ·
[Architecture](docs/ARCHITECTURE.md) · [API](docs/API.md) · [Format](docs/FORMAT.md).

**MIT 许可 · 规格优先 · 开放治理。** 欢迎贡献——见 [CONTRIBUTING.md](CONTRIBUTING.md)。

</div>
