# FLUXLOOP — the reference edge device

[**FLUXLOOP**](https://github.com/DataFlux-Robot/FLUXLOOP) is the first hardware
platform that natively reads and writes `.flux` DevReady assets **on-device**.
It is an STM32-based board running an RTOS (FreeRTOS), and it is the reference
edge device for the "operate" phase of the DevReady lifecycle.

## The relationship

```
Cloud GPU                     FLUXLOOP (STM32, MCU)
─────────────                 ─────────────────────
generate .flux                operate .flux
  (design, sim, VLA)            (read body, append PHM journal)
        │                              │
        └──────── same .flux ──────────┘
                 (byte-identical)
```

- **Generate**: on the cloud, a `.flux` is created (body from URDF/USD, mind from
  task specs). FLUXmeme's Tier-2 C middleware runs here.
- **Reuse**: the same `.flux` projects to USD (sim), OKF (VLA), A2A (orchestration).
- **Operate**: the same `.flux` is flashed to FLUXLOOP. The MCU reads body params
  (joint limits, device topology), runs the robot, and **appends telemetry/PHM to
  the journal** — growing the same file on-device.

## Why FLUXLOOP defines the complexity budget

> **If it doesn't fit on the MCU, it doesn't ship.**

FLUXmeme's Tier-2 middleware has a hard complexity ceiling: it must run on a
Cortex-M7 with limited RAM/flash. This constraint is the **razor** that keeps the
core honest:

- No dynamic allocation in hot paths (or minimal)
- No heavyweight dependencies (OpenUSD, Python, full LIVRPS are Tier-3, excluded)
- The same C code compiles for desktop AND MCU — **byte-identical `.flux`**

This isn't "edge-ready" as a marketing slogan. It's edge-ready because the code
literally runs on the metal.

## Planned FLUXLOOP milestones

- [ ] `platform/stm32/` — FreeRTOS + CubeMX CMake + port_flash.c
- [ ] Flash backend: write real `.flux` bytes to flash (FlashDB wear-leveling)
- [ ] On-device demo: read body params + append PHM signal to journal
- [ ] Random-read benchmark (footer index in RAM + binary search)
- [ ] ESP32 port (reuse same port layer)

---

# FLUXLOOP —— 参考边缘设备(中文)

[**FLUXLOOP**](https://github.com/DataFlux-Robot/FLUXLOOP) 是首个**原生读写 `.flux`
DevReady 资产**的硬件平台。它是一块基于 STM32、跑 RTOS(FreeRTOS)的开发板,是
DevReady 生命周期"运维"阶段的参考边缘设备。

## 关系

```
云端 GPU                     FLUXLOOP (STM32, MCU)
─────────────                 ─────────────────────
生成 .flux                    运维 .flux
  (设计、仿真、VLA)              (读身体,追加 PHM 日志)
        │                              │
        └──────── 同一个 .flux ────────┘
                 (字节级一致)
```

- **生成**:云端创建 `.flux`(URDF/USD → 身体;任务规格 → 心智)
- **复用**:同一个 `.flux` 投影到 USD(仿真)、OKF(VLA)、A2A(编排)
- **运维**:同一个 `.flux` 烧到 FLUXLOOP。MCU 读身体参数(关节限位、设备拓扑),
  运行机器人,并**在设备上追加遥测/PHM 到日志**——同一个文件在生长。

## FLUXLOOP 定义复杂度上限

> **放不进 MCU 的,就不发版。**

FLUXmeme 的 Tier-2 中间件有硬性复杂度上限:必须跑在 Cortex-M7 上(有限 RAM/flash)。
这个约束是让核心保持诚实的**剃刀**:
- 热路径无动态分配(或极少)
- 无重依赖(OpenUSD、Python、完整 LIVRPS 是 Tier-3,排除)
- 同一份 C 代码编译桌面 AND MCU——**字节相同的 `.flux`**

这不是营销口号。是代码真正跑在裸金属上。

## FLUXLOOP 计划里程碑

- `platform/stm32/` — FreeRTOS + CubeMX CMake + port_flash.c
- Flash 后端:写真实 `.flux` 字节到 flash(FlashDB 磨损均衡)
- 真机 demo:读身体参数 + 追加 PHM 信号到日志
- 随机读基准(footer 索引常驻 RAM + 二分)
- ESP32 移植(复用同一 port 层)
