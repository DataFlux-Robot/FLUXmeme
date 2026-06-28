# Governance

FLUXmeme is MIT-licensed and currently **maintainer-led**, with a roadmap to move
to **neutral, community governance** under a foundation (Linux Foundation / ASWF)
as the ecosystem grows.

## Open governance (day-1)

The project ships these governance artifacts from day one:

| File | Purpose |
|---|---|
| [CONTRIBUTING.md](https://github.com/DataFlux-Robot/FLUXmeme/blob/main/CONTRIBUTING.md) | How to contribute: build/test/style/PR process, CLA |
| [CODE_OF_CONDUCT.md](https://github.com/DataFlux-Robot/FLUXmeme/blob/main/CODE_OF_CONDUCT.md) | Contributor Covenant 2.1 |
| [SECURITY.md](https://github.com/DataFlux-Robot/FLUXmeme/blob/main/SECURITY.md) | Vulnerability reporting + trust model |
| [docs/rfc/](https://github.com/DataFlux-Robot/FLUXmeme/blob/main/docs/rfc/) | RFC process for format changes |

## RFC process

Format-level changes (wire layout, record fields, canonical facets, composition
semantics) go through the **RFC process**:

1. Copy `docs/rfc/0000-template.md` to `NNNN-name.md`
2. Open a PR titled `RFC NNNN: ...`
3. Discussion on the PR
4. Accepted → implemented in a follow-up PR (bumps format version)

Fields are **add-only** (forward/backward compatible). Removals follow a
deprecation cycle.

## Semantic versioning

- `v1.0.0` — first stable format + reference SDK
- Format changes bump `fmt_version` in the header; field additions are compatible
- See [CHANGELOG.md](https://github.com/DataFlux-Robot/FLUXmeme/blob/main/CHANGELOG.md)

## Foundation roadmap

As the ecosystem grows, FLUXmeme aims to:

1. Donate to **Linux Foundation / ASWF** (like OpenUSD → ASWF)
2. Separate spec from reference implementation (independent spec repo)
3. Multi-vendor governance board
4. Trademark / naming policy under the foundation

## Trademarks

"FLUXmeme" and the `.flux` / `.fluxa` extensions refer to this format. The spec
is vendor-neutral. See CONTRIBUTING.md for details.

---

# 治理(中文)

FLUXmeme 是 MIT 许可,当前**维护者主导**,路线是随生态增长转向**中立社区治理**
(Linux Foundation / ASWF)。

## 开放治理(day-1)

| 文件 | 目的 |
|---|---|
| CONTRIBUTING.md | 如何贡献:构建/测试/风格/PR 流程 |
| CODE_OF_CONDUCT.md | Contributor Covenant 2.1 |
| SECURITY.md | 漏洞报告 + 信任模型 |
| docs/rfc/ | 格式变更 RFC 流程 |

## RFC 流程

格式级变更(wire 布局、记录字段、canonical facet、组合语义)走 **RFC**:

1. 复制模板 `docs/rfc/0000-template.md` → `NNNN-name.md`
2. 开 PR 标题 `RFC NNNN: ...`
3. PR 上讨论
4. 接受 → 后续 PR 实现(提升格式版本)

字段**只增不删**(前向/后向兼容)。删除走 deprecation 周期。

## 语义版本

- `v1.0.0` — 第一个稳定格式 + 参考 SDK
- 格式变更提升头中的 `fmt_version`;字段追加兼容
- 见 [CHANGELOG.md](https://github.com/DataFlux-Robot/FLUXmeme/blob/main/CHANGELOG.md)

## Foundation 路线

1. 捐赠到 **Linux Foundation / ASWF**(类 OpenUSD → ASWF)
2. spec 与参考实现分离(独立 spec repo)
3. 多厂商治理委员会
4. 商标/命名政策归属 foundation
