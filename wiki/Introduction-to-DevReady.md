# Introduction to DevReady

## What is DevReady?

**DevReady** (Development-Ready) is an asset standard that upgrades NVIDIA's
SimReady. SimReady made 3D assets *physically accurate* (measured mass, friction,
deformation). DevReady makes them **cognitively and operationally complete**, and
**edge-portable**.

A DevReady asset carries, in **one file**:

| Layer | What it holds | Projects to |
|---|---|---|
| **BODY** | Robot structure: graph kinematics (closed-loop), geometry, device-comm topology | USD |
| **MIND** | Task knowledge (markdown), agent skills (JSON) | OKF + A2A |
| **JOURNAL** | Lifetime telemetry, PHM health record (grows on-device) | MCAP + MAVLink |

## Why DevReady exists

Today, a robot is described by **5+ sidecar files** that drift apart:

```
robot.urdf  +  scene.usd  +  docs/*.md  +  agent.json  +  log.mcap  +  config.yaml
```

No single tool understands all of them. None runs on the robot. They diverge over
the robot's life. DevReady solves this: **one `.flux` = the whole robot, through
its entire lifecycle**.

## SimReady vs DevReady

| | SimReady | DevReady |
|---|---|---|
| **Carries** | scene + physics | scene + **knowledge + agent + lifetime journal** |
| **Agent-native** | no (needs runtime) | **yes** (self-describing, `flux inspect`) |
| **Composition** | USD layers (scene-centric) | **LIVRPS** (decentralized, each robot = autonomous node) |
| **Robot model** | tree (URDF) | **graph, closed loops first-class** |
| **Edge/MCU** | no (desktop-only) | **yes** (lean C on Cortex-M7) |
| **Lifecycle** | born in sim, dies in sim | **born in reality, perpetually real** |

## The lifecycle

```
GENERATE → REUSE → OPERATE → (loop forever)
```

1. **Generate**: a `.flux` is born (design → body; add knowledge; add skills)
2. **Reuse**: project to USD (sim), OKF (VLA), A2A (orchestration) — all from one source
3. **Operate**: the same file accrues telemetry + PHM on-device (MCU)
4. The file **is** the digital thread. No sidecar drift.

## Who benefits?

- **Robotics teams**: one artifact replaces the drifting folder
- **Sim platforms** (Newton, Isaac Lab, Gazebo): FLUXmeme is the single source behind USD/MCAP
- **VLA / agent startups**: agents read assets without a runtime
- **Standards bodies**: MIT, spec-first, open governance, foundation-bound

---

# DevReady 入门(中文)

## DevReady 是什么?

**DevReady**(开发就绪)是一种资产标准,是 NVIDIA SimReady 的升级版。SimReady 让 3D 资产
*物理准确*(实测质量、摩擦、形变)。DevReady 让它们**认知与操作完整**,且**可上边缘**。

一个 DevReady 资产在**一个文件**里承载:

| 层 | 承载 | 投影到 |
|---|---|---|
| **BODY** | 机器人结构:graph 运动学(闭链)、几何、设备拓扑 | USD |
| **MIND** | 任务知识(markdown)、agent 技能(JSON) | OKF + A2A |
| **JOURNAL** | 终身遥测、PHM 健康档案(设备上生长) | MCAP + MAVLink |

## 为什么需要 DevReady

今天,一个机器人被 **5+ 个 sidecar 文件**描述,它们会漂移分离:

```
robot.urdf  +  scene.usd  +  docs/*.md  +  agent.json  +  log.mcap  +  config.yaml
```

没有一个工具同时理解所有文件。没有一个能在机器人本体上运行。它们随机器人寿命漂移。
DevReady 解决这个问题:**一个 `.flux` = 整个机器人,贯穿全生命周期**。

## 生命周期

```
生成 → 复用 → 运维 → (循环)
```

1. **生成**:`.flux` 诞生(设计→身体;加知识;加技能)
2. **复用**:投影到 USD(仿真)、OKF(VLA)、A2A(编排)——一源多投影
3. **运维**:同一个文件在设备上(MCU)累积遥测 + PHM
4. 文件**就是**数字主线。无 sidecar 漂移。

## 谁受益?

- **机器人团队**:一个工件替换漂移的文件夹
- **仿真平台**(Newton、Isaac Lab、Gazebo):FLUXmeme 是 USD/MCAP 背后的单一源
- **VLA / agent 初创**:agent 无需运行时即可读资产
- **标准化导向**:MIT、spec 优先、开放治理
