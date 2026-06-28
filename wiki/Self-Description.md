# Self-Description (Agent-Native)

A `.flux` file is **self-describing**: any tool, agent, or human can understand
its contents **without a runtime, plugin, or external dependency**.

## How it works

1. **Magic bytes** `FLXM` in the 64-byte header — `flux inspect` identifies any file
2. **Schema ID** in the header — format version + schema hash
3. **Records carry `kind` + `meta`** — human-readable key/value pairs; an agent
   understands the content by reading field names (`name`, `title`, `type`, `tags`)
4. **`.fluxa` text form** — the whole store in grep-able, diff-able plain text

## Why this matters for agents

An AI agent (LLM, VLA) can:

```python
from fluxmeme import Store

with Store("robot.flux") as s, s.read() as txn:
    for r in s.scan(txn):
        print(r.kind, r.meta)
        if r.payload:
            print(r.payload[:200])
```

Without installing a simulation runtime, a USD plugin, or a ROS stack — the agent
opens the `.flux`, scans records, and understands: *"this is a robot with 2 links,
a revolute joint, and a cart-pole balancing task."*

## Compare to USD

| | USD | .flux |
|---|---|---|
| Agent reads without runtime? | no (needs USD plugin + scene graph) | **yes** |
| Human-grepable? | no (binary USDC; USDA is verbose) | **yes** (`.fluxa`) |
| Self-identifying? | yes (magic + UsdStage) | **yes** (magic `FLXM` + schema_id) |
| Structured metadata per record? | yes (prim attributes, but engine-heavy) | **yes** (simple `kind` + `meta` KV) |

## The `.fluxa` text form

```
#FLUXMEME 1.0
R 019f0411ac9819354883672df8f5d3ec
L MIND
K concept
M title=Cart-pole balancing task
M tags=rl,control,balance
D 73
# Cart-Pole Balancing
Balance the pole upright...
```

This is **the canonical source** (what you commit to Git). Agents and humans read
it like any text file. `flux conv` converts to/from the binary `.flux`.

---

# 自描述(Agent 原生)(中文)

一个 `.flux` 文件是**自描述**的:任何工具、agent 或人都能理解其内容,**无需运行时、
插件或外部依赖**。

## 工作原理

1. **Magic 字节** `FLXM`(64 字节头)——`flux inspect` 识别任意文件
2. **Schema ID**(头)——格式版本 + schema 哈希
3. **记录带 `kind` + `meta`**——人可读的 KV;agent 读字段名即可理解内容
4. **`.fluxa` 文本形态**——整个 store 可 grep、可 diff 的纯文本

## 为什么这对 agent 重要

AI agent(LLM、VLA)可以:

```python
with Store("robot.flux") as s, s.read() as txn:
    for r in s.scan(txn):
        print(r.kind, r.meta)
```

无需安装仿真运行时、USD 插件或 ROS——agent 打开 `.flux`、扫描记录,就理解了:
*"这是一个有 2 个连杆、1 个旋转关节的机器人,任务是平衡杆。"*

## 对比 USD

| | USD | .flux |
|---|---|---|
| Agent 无运行时可读? | 否(需 USD 插件 + 场景图) | **是** |
| 人可 grep? | 否 | **是**(`.fluxa`) |
| 自识别? | 是(magic + UsdStage) | **是**(magic `FLXM`) |
| 每条记录有结构化元数据? | 是(prim 属性,但引擎重) | **是**(简单 `kind` + `meta`) |
