<div align="center">

# FLUXmeme

**具身节点的自描述资产格式 · Self-describing asset format for embodied nodes**

一个机器人 = 一个 `.flux` = **身体 + 心智 + 终身日志**<br>
One robot = one `.flux` = **body + mind + lifetime journal**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Language: C11](https://img.shields.io/badge/language-C11-blue.svg)]()
[![Spec](https://img.shields.io/badge/spec-v1.0--draft-orange.svg)](SPEC.md)
[![Status](https://img.shields.io/badge/status-v0.1%20slice-red.svg)]()

</div>

> ⚙️ **状态 / Status:** v0.1 参考实现切片 —— 桌面核心引擎 + OKF 转录往返。MCU(FLUXLOOP/STM32H7)、Python、其余转录器按里程碑推进。桌面路径已可构建并通过测试。

---

## 🌱 这是什么 · What it is

一个机器人今天被拆成 5+ 个互不相识、各自漂移的 sidecar 文件(URDF 身体、USD 场景、markdown 知识、JSON agent 卡、MCAP 日志、YAML 配置)。FLUXmeme 把它们**统一成一个自描述工件**,从 64 位桌面到 Cortex-M7 MCU 都能读写,**字节级一致**。

**心智模型 —— 一个 `.flux` 是一个具身节点(身份证 + 身体 + 心智 + 健康档案):**

| `layer` | 本性 / Nature | 转录目标 / Transcodes to |
|---|---|---|
| **BODY** | graph 运动学(闭链,超越 URDF)+ 几何/惯量 + affordance + 设备拓扑 | → USD |
| **MIND** | 任务知识(OKF markdown)+ agent 技能(A2A) | → OKF + A2A |
| **JOURNAL** | 遥测/参数 + PHM 黑匣子(随寿命生长,多时钟域) | → MCAP + MAVLink |

**A robot today is scattered across 5+ drifting sidecar files. FLUXmeme unifies them into one self-describing artifact, readable/writable from a 64-bit desktop down to a Cortex-M7 MCU, byte-for-byte identical.**

核心特性 / Key properties:
- **哑存储 / dumb storage** — 只存 TEXT/BIN blob + 结构化元数据;解析/渲染/求解都在应用层
- **MVCC** — 无锁多读者 + 单写者串行 + 乐观 CAS + FIFO cursor
- **LIVRPS 组合 / composition** — 去中心化多节点,非破坏 override
- **双形态 / dual form** — `.fluxa`(文本,canonical 源,Git 友好)↔ `.flux`(flatcc 二进制,编译产物)
- **自描述 / self-describing** — `flux inspect` 即可识别,agent 无需运行时即可读写

详见 / See [SPEC.md](SPEC.md)。

---

## 🚀 快速开始 · Quickstart (desktop)

**前置 / Prereq:** CMake ≥ 3.16 + MSVC(VS 2022)/ GCC / Clang。

```bat
:: Windows, VS 2022 Developer prompt (or the VS-bundled CMake):
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\bin\Release\demo_okf.exe          :: store -> OKF -> store 往返
build\bin\Release\test_crc32c.exe
build\bin\Release\test_record_codec.exe
build\bin\Release\test_store_mvcc.exe
```

```bash
# Linux / macOS:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/demo_okf
```

`demo_okf` 演示完整往返:写入 3 个 MIND/concept 记录(A 链接到 B)→ 转录成 OKF markdown bundle → 回灌进新 store → 断言标题/payload/链接全部恢复。

---

## 🧩 用法示例 · Usage (C)

```c
#include "fluxmeme/fluxmeme.h"

flux_store_t* s;
flux_open("robot.flux", 1, &s);

flux_txn_t* w; flux_txn_begin_write(s, &w);
flux_meta_kv_t meta[2] = { {"title","Base"}, {"type","link"} };
flux_record_t r = {0};
r.layer = FLUX_LAYER_BODY; r.kind = "link";
r.meta = meta; r.meta_count = 2;
r.payload.data = (const uint8_t*)"base link"; r.payload.len = 9;
flux_put(w, &r);
flux_txn_commit(w);

/* transcode the MIND layer to an OKF markdown bundle */
flux_txn_t* rd; flux_txn_begin_read(s, &rd);
flux_to_okf(rd, "okf_out/");
flux_close(s);
```

---

## 🗺️ 路线 / Roadmap

- ✅ **v0.1** 桌面核心引擎(append log + MVCC + cursor + CAS)+ OKF 转录往返
- 🚧 **v1.0** A2A / USD 转录 + robot-graph(device-comm)+ `.fluxa`/conv + CLI + Python
- 🔜 **v1.1** MAVLink 帧模式 + 完整 LIVRPS
- 🔜 **v1.2** CAN / EtherCAT
- 🔜 **v1.3** FLUXLOOP(STM32H7)on-device 全读写 + 多语言 SDK(Rust/JS/ROS2)

---

## 🤝 治理 · Governance

MIT 许可。开放治理 day-1(社区 → 瞄 Linux Foundation / ASWF)。

**wedge:** 一个自描述资产 = 场景(USD)+ 知识(OKF)+ agent(A2A)+ 信号(MAVLink),原生组合,agent 直接读写 —— 一个资产即一个可推理/可操作单元。
