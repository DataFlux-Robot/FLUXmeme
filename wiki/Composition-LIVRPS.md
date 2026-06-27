# Composition (LIVRPS)

FLUXmeme borrows USD's **composition arcs** but decentralizes them: each `.flux`
is an autonomous embodied node, and any node can act as a composition root. The
result is a **read-time, non-destructive merged view** — the original files are
never modified.

> **LIVRPS** = the fixed opinion-strength order:
> **L**ocal > **I**nherits > **V**ariants > **R**eferences > **P**ayloads > **S**pecializes

---

## How it works

### 1. Layer stacks (sublayers)

A root `.flux` declares its sublayers via a special record:

```python
from fluxmeme import Store, Record, LAYER_MIND

with Store("root.flux", writable=True) as s:
    with s.write() as txn:
        s.put(txn, Record(
            layer=LAYER_MIND, kind="flux/compose",
            meta={"sublayers": "override.flux;base.flux"},  # strongest first
        ))
```

The root is the strongest layer; sublayers are listed strongest-first. When you
open the composed view:

```bash
flux compose root.flux
```

...the engine opens all layers and resolves each record `id` across the stack.

### 2. Field-level merge

When the same record `id` appears in multiple layers, fields merge — not
whole-record override:

```
base.flux:    {name="base", mass="10", color="red"}
override.flux: {                          mass="20"          }
─────────────────────────────────────────────────────────────
merged view:   {name="base", mass="20", color="red"}
```

- **Strongest layer that provides a field wins** (override's mass=20)
- **Weaker layers fill gaps** the stronger doesn't have (base's name, color)

This is more powerful than record-level override (which would lose `name` and
`color` when override replaces the whole record).

#### Code example

```python
from fluxmeme import Store, Record, LAYER_BODY

# Base: full robot link
with Store("base.flux", writable=True) as s:
    with s.write() as txn:
        r = Record(layer=LAYER_BODY, kind="robot/link",
                   meta={"name": "base", "mass": "10", "color": "red"})
        s.put(txn, r)
        link_id = r.id

# Override: only mass changes
with Store("override.flux", writable=True) as s:
    with s.write() as txn:
        r = Record(layer=LAYER_BODY, kind="robot/link", meta={"mass": "20"})
        r.id = link_id  # same id -> field-level merge
        s.put(txn, r)

# Root: declare the stack (override first = stronger)
with Store("root.flux", writable=True) as s:
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_MIND, kind="flux/compose",
                          meta={"sublayers": "override.flux;base.flux"}))
```

Resolve:
```bash
flux compose root.flux
# Output: mass=20 (override wins), name=base + color=red (base fills gaps)
```

### 3. Variants

A record can be tagged with a **variant set** + **variant value**:

```python
# Heavy variant
s.put(txn, Record(layer=LAYER_BODY, kind="robot/link",
    meta={"flux_variant_set": "config", "flux_variant": "heavy", "mass": "50"}))

# Light variant
s.put(txn, Record(layer=LAYER_BODY, kind="robot/link",
    meta={"flux_variant_set": "config", "flux_variant": "light", "mass": "5"}))
```

Select which variant is active:

```python
from fluxmeme import Store
# CLI:  flux compose root.flux --variant config=heavy
# C:    flux_compose_set_variant(c, "config", "heavy")
```

Rules:
- Tagged records with **matching** set+value become **active** (stronger than base)
- Tagged records with **non-matching** value are **hidden**
- Untagged records are **always active** (the base)

### 4. Inherit arc

A record can **inherit** from another record's fields:

```python
# Base "class" record
base = Record(layer=LAYER_BODY, kind="robot/link",
              meta={"name": "BaseLink", "color": "blue", "mass": "1.0"})
s.put(txn, base)

# Child that inherits base's fields it doesn't override
child = Record(layer=LAYER_BODY, kind="robot/link",
               meta={"name": "ArmLink", "flux_inherit": base.id})
s.put(txn, child)
# Merged child: name=ArmLink (own), color=blue (inherited), mass=1.0 (inherited)
```

The `flux_inherit=<id>` meta tells the composition engine to pull the base
record's fields as **weaker** — they fill gaps the child doesn't have. One level
deep (no recursion).

### 5. The LIVRPS strength order

When multiple sources express an opinion on the same field, resolution follows
the fixed strength order:

```
Local (the layer itself)
  > Inherits (flux_inherit arcs)
    > Variants (variant set selections)
      > References (sublayer references)
        > Payloads (lazy-loaded references)
          > Specializes (class specialization arcs)
```

In practice, v0.1 implements: **Local (sublayers) + Variants + Inherits + field
merge**. This covers the 80% use cases. Full `specializes` / lazy `payload` arcs
are planned for v1.1+.

### 6. Non-destructive

Composition is a **read-time virtual merge**:

- The original layer files are **never modified**
- `flux_compose_open(root)` returns a view; close it and the layers are unchanged
- Write to the composed view is not supported (read-only; writes go to a specific
  layer via `flux_open(layer_path)`)

### 7. Decentralized

Each `.flux` is one autonomous, self-rooted node. **Any node can be a composition
root** — there is no global scene authority:

```
Node A composes: [A, B, C]  -> A's view of the world
Node B composes: [B, A, D]  -> B's view (different!)
```

This matches how robots actually work (each robot is its own entity), not how a
sim scene graph works (one centralized tree).

---

## API reference

### C (`src/compose/compose.h`)

```c
flux_compose_t* c;
flux_compose_open("root.flux", &c);

// Select a variant
flux_compose_set_variant(c, "config", "heavy");

// Resolve one record (field-level merge across all active layers)
flux_record_t rec;
flux_compose_get(c, &id, &rec);

// Scan the merged view
flux_compose_iter_t* it;
flux_compose_scan(c, NULL, &it);
while (flux_compose_iter_next(it, &rec) == FLUX_OK) { ... }

flux_compose_close(c);
```

### Python (CLI equivalent)

```bash
flux compose root.flux
flux compose root.flux --variant config=heavy
```

### Python (programmatic — planned)

The `flux_compose_*` C functions are not yet bound to the Python `Store` class.
Use the CLI (`flux compose`) or call the C API directly. Python binding is a
tracked TODO.

---

## Common patterns

### Pattern: configuration override

```python
# base.flux: default robot
# experiment_a.flux: overrides joint limits (same ids)
# experiment_b.flux: overrides mass (same ids)

# root_a:  sublayers = experiment_a;base
# root_b:  sublayers = experiment_b;base
# Compare both without modifying base:
#   flux compose root_a.flux
#   flux compose root_b.flux
```

### Pattern: variant selection (robot configurations)

```python
# robot.flux has:
#   base link (no variant tag) -- always active
#   heavy link (variant=config:heavy) -- active when heavy selected
#   light link (variant=config:light) -- active when light selected

# CLI:
flux compose robot.flux --variant config=heavy   # mass=50
flux compose robot.flux --variant config=light   # mass=5
flux compose robot.flux                           # mass=10 (base only)
```

### Pattern: multi-robot assembly

```python
# Each robot is an autonomous .flux node.
# A "world" root composes them (decentralized):
with Store("world.flux", writable=True) as s:
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_MIND, kind="flux/compose",
            meta={"sublayers": "robot_a.flux;robot_b.flux;robot_c.flux"}))
```

---

# 组合 LIVRPS(中文)

FLUXmeme 借鉴 USD 的**组合弧**,但去中心化:每个 `.flux` 是一个自治的具身节点,任意节点
都可以作组合根。结果是**读时、非破坏的虚拟合并视图**——原始文件永远不被修改。

> **LIVRPS** = 固定意见强度序:
> **L**ocal(本地)> **I**nherits(继承)> **V**ariants(变体)> **R**eferences(引用)>
> **P**ayloads(载荷)> **S**pecializes(特化)

---

## 工作原理

### 1. 层栈(sublayers)

根 `.flux` 通过一条特殊记录声明子层:

```python
s.put(txn, Record(
    layer=LAYER_MIND, kind="flux/compose",
    meta={"sublayers": "override.flux;base.flux"},  # 最强在前
))
```

根是最强层;子层按最强在前列出。打开组合视图:

```bash
flux compose root.flux
```

### 2. 字段级合并

同一个记录 `id` 出现在多层时,字段**逐个合并**——不是整条记录覆盖:

```
base.flux:    {name="base", mass="10", color="red"}
override.flux: {                          mass="20"          }
合并视图:       {name="base", mass="20", color="red"}
```

- **强层有该字段则赢**(override 的 mass=20)
- **弱层补缺**(base 的 name、color 被保留)

### 3. Variants(变体)

记录可标记**变体集 + 变体值**:

```python
# 重型变体
Record(meta={"flux_variant_set": "config", "flux_variant": "heavy", "mass": "50"})
# 轻型变体
Record(meta={"flux_variant_set": "config", "flux_variant": "light", "mass": "5"})
```

选择激活哪个变体:`flux compose root.flux --variant config=heavy`

规则:
- 标记记录**匹配**则激活(比 base 更强)
- 标记记录**不匹配**则隐藏
- 未标记记录**总是活跃**(base)

### 4. Inherit 弧(继承)

记录可从另一条记录**继承**字段:

```python
base = Record(meta={"name": "BaseLink", "color": "blue", "mass": "1.0"})
child = Record(meta={"name": "ArmLink", "flux_inherit": base.id})
# 合并 child:name=ArmLink(自有), color=blue(继承), mass=1.0(继承)
```

`flux_inherit=<id>` 指示组合引擎拉取 base 记录的字段作为**弱者**(填缺)。一级深度。

### 5. LIVRPS 强度序

```
Local(层本身)
  > Inherits(flux_inherit 弧)
    > Variants(变体集选择)
      > References(子层引用)
        > Payloads(懒加载引用)
          > Specializes(类特化弧)
```

v0.1 实现:**Local(sublayers)+ Variants + Inherits + 字段合并**。完整
specializes / 懒加载 payload 弧计划 v1.1+。

### 6. 非破坏

组合是**读时虚拟合并**:
- 原始层文件**永远不被修改**
- `flux_compose_open(root)` 返回视图;关闭后层不变
- 不支持写组合视图(只读;写入通过 `flux_open(layer_path)` 到特定层)

### 7. 去中心化

每个 `.flux` 是一个自治、自根的节点。**任意节点可作组合根**——无全局场景权威:

```
节点 A 组合: [A, B, C]  -> A 视角的世界
节点 B 组合: [B, A, D]  -> B 视角(不同!)
```

这契合机器人真实的样子(每个机器人独立),而非仿真场景图(一棵中心化树)。

## 常见模式

### 配置覆盖

```python
# base.flux: 默认机器人
# experiment_a.flux: 覆盖关节限位(相同 id)
# 不改 base,对比两个实验:
flux compose root_a.flux
flux compose root_b.flux
```

### 变体选择(机器人配置)

```bash
flux compose robot.flux --variant config=heavy   # mass=50
flux compose robot.flux --variant config=light   # mass=5
flux compose robot.flux                           # mass=10(仅 base)
```

### 多机器人装配

```python
# 每个机器人是一个自治 .flux 节点。
# 一个"世界"根组合它们(去中心化):
s.put(txn, Record(kind="flux/compose",
    meta={"sublayers": "robot_a.flux;robot_b.flux;robot_c.flux"}))
```
