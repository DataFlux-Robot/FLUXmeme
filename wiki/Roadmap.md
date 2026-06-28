# Roadmap

## Done (v0.1)

- Core engine: append-only log + MVCC + CRC32C + ULID + tombstones + compact
- 5 transcoders: USD / OKF / A2A / MCAP / MAVLink (all round-trip tested)
- `.fluxa` text canonical source + `flux conv` (lossless bidirectional)
- LIVRPS composition: sublayers + field-level merge + variants + inherit arc
- Robot facets: robot-graph (closed-loop cycle detection) + device-comm (protocol routing)
- URDF/SDF import (with dynamics: limit/axis) + SimReady -> DevReady skill
- CLI: inspect / dump / transcode / conv / compose / from-simready
- Python: `pip install` (scikit-build-core), ctypes, validated (pytest 2/2)
- Governance: CONTRIBUTING / COC / SECURITY / RFC process
- Docs: SPEC / FORMAT / API / ARCHITECTURE / CONCEPTS / GETTING_STARTED / BENCHMARK
- CI: GitHub Actions (Win MSVC + Linux gcc)
- 12 demos + 4 unit tests — ALL PASS on MSVC
- Visualization tool (flux_viz.py — matplotlib record graph + layer bars)
- Newton integration demo (USD <-> .flux, conditional sim)

## In progress (v1.0)

- [ ] Tag v1.0.0 + GitHub Release
- [ ] Finalize SPEC (normative review)
- [ ] Footer index persistence (on-disk for MCU random access)
- [ ] More example assets (quadruped, humanoid, soft-body)

## Planned (v1.x)

### v1.1 — Composition & transport
- [ ] MJCF (MuJoCo) import
- [ ] Full specializes arc (currently covered by inherit)
- [ ] CAN / EtherCAT frame codecs (same codec registry)

### v1.2 — Platform SDKs
- [ ] Rust bindings
- [ ] JavaScript / WASM bindings
- [ ] ROS 2 (micro-ROS) integration

### v1.3 — Embedded (FLUXLOOP)
- [ ] STM32H7 port (FreeRTOS, multi-RTOS HAL)
- [ ] Flash backend (write real .flux to flash, FlashDB wear-leveling)
- [ ] On-device full read/write + PHM hero demo
- [ ] ESP32 port (reuse same port layer)

### Future
- [ ] OpenUSD C++ backend (optional, high-fidelity USDA import/export)
- [ ] mmap backend (desktop zero-copy)
- [ ] blake3 content hash (replace placeholder)
- [ ] Foundation donation (Linux Foundation / ASWF)
- [ ] DCC plugins (Blender, Houdini, Maya)
- [ ] Interactive viewer (web-based, WebGL)

---

# 路线(中文)

## 已完成 (v0.1)

- 核心引擎:append-only log + MVCC + CRC32C + ULID + tombstone + compact
- 5 个 transcoder:USD / OKF / A2A / MCAP / MAVLink(全往返测试)
- `.fluxa` 文本 canonical 源 + `flux conv`(无损双向)
- LIVRPS 组合:sublayers + 字段级合并 + variants + inherit 弧
- Robot facet:robot-graph(闭链环检测)+ device-comm(协议路由)
- URDF/SDF 导入(含动力学 limit/axis)+ SimReady -> DevReady skill
- CLI:inspect / dump / transcode / conv / compose / from-simready
- Python:`pip install`(scikit-build-core),ctypes,已验证(pytest 2/2)
- 治理:CONTRIBUTING / COC / SECURITY / RFC 流程
- 文档:SPEC / FORMAT / API / ARCHITECTURE / CONCEPTS / BENCHMARK
- CI:GitHub Actions(Win MSVC + Linux gcc)
- 12 demo + 4 单元测试——全 PASS
- 可视化工具(flux_viz.py)
- Newton 集成 demo

## 进行中 (v1.0)

- [ ] 打 v1.0.0 tag + GitHub Release
- [ ] SPEC 定稿
- [ ] footer 索引落盘(MCU 随机访问)
- [ ] 更多示例资产

## 计划 (v1.x)

### v1.1 — 组合与传输
- MJCF 导入 / 完整 specializes 弧 / CAN / EtherCAT 帧 codec

### v1.2 — 平台 SDK
- Rust / JavaScript(WASM)/ ROS 2(micro-ROS)

### v1.3 — 嵌入式 (FLUXLOOP)
- STM32H7 移植(FreeRTOS,多 RTOS HAL)
- Flash 后端(写真实 .flux 到 flash)
- 真机全读写 + on-device PHM hero
- ESP32 移植

### 未来
- OpenUSD C++ 后端(可选,高保真 USDA)
- mmap 后端(桌面零拷贝)
- blake3 内容哈希
- Foundation 捐赠(Linux Foundation / ASWF)
- DCC 插件(Blender / Houdini / Maya)
- 交互式查看器(WebGL)
