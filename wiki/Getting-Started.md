# Getting Started

## Install

```bash
pip install git+https://github.com/DataFlux-Robot/FLUXmeme.git
```

scikit-build-core compiles the C library automatically. Works on Windows (MSVC),
Linux (gcc), macOS (clang). On Windows, use a VS 2022 Developer prompt if the
compiler isn't found.

## Your first .flux (60 seconds)

```python
from fluxmeme import Store, Record, LAYER_BODY, LAYER_MIND, LAYER_JOURNAL

with Store("my_robot.flux", writable=True) as s:
    with s.write() as txn:
        # BODY: a robot link
        s.put(txn, Record(layer=LAYER_BODY, kind="robot/link", meta={"name": "base"}))
        # MIND: a task concept
        s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                          meta={"title": "Pick and place"},
                          payload=b"# Pick objects, place on target."))
        # JOURNAL: a telemetry signal
        s.put(txn, Record(layer=LAYER_JOURNAL, kind="signal",
                          meta={"name": "battery", "value": "12.4"}))

# Inspect
with Store("my_robot.flux") as s, s.read() as txn:
    print("BODY:   ", len(list(s.scan(txn, layer=LAYER_BODY))), "records")
    print("MIND:   ", len(list(s.scan(txn, layer=LAYER_MIND))), "records")
    print("JOURNAL:", len(list(s.scan(txn, layer=LAYER_JOURNAL))), "records")
```

Output:
```
BODY:    1 records
MIND:    1 records
JOURNAL: 1 records
```

All three natures — body + mind + journal — in ONE file.

## Project to standard formats

```python
with Store("my_robot.flux") as s, s.read() as txn:
    s.to_usd(txn, "robot.usda")       # BODY -> USD
    s.to_okf(txn, "okf_out/")         # MIND -> OKF markdown
    s.to_mcap(txn, "run.mcap")        # JOURNAL -> MCAP rosbag2
    s.to_fluxa(txn, "robot.fluxa")    # whole store -> text (for Git)
```

## Import an existing robot

```python
# From URDF (with joint limits + axis)
with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.from_urdf(txn, "my_robot.urdf")

# From USD/SimReady
with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.from_usd(txn, "scene.usda")
```

## CLI

```bash
flux inspect robot.flux          # identify + stats
flux dump robot.flux             # list all records
flux transcode robot.flux usd out.usda   # .flux -> USD
flux conv robot.flux robot.fluxa         # .flux -> .fluxa text
flux compose root.flux                    # resolve LIVRPS merged view
flux from-simready scene.usda robot.flux  # SimReady -> DevReady
```

## Run the demos

```bash
python demo/demo_newton.py              # USD <-> .flux + Newton integration
python demo/demo_newton.py --newton     # with Newton physics (if installed)
```

## Next steps

- [Tutorials](Tutorials) — guided walkthroughs
- [Introduction to .flux](Introduction-to-.flux) — format internals
- [Format Specification](Format-Specification) — binary layout
- [API Reference](API-Reference) — complete C + Python docs

---

# 快速开始(中文)

## 安装

```bash
pip install git+https://github.com/DataFlux-Robot/FLUXmeme.git
```

scikit-build-core 自动编译 C 库。支持 Windows(MSVC)、Linux(gcc)、macOS(clang)。

## 你的第一个 .flux(60 秒)

```python
from fluxmeme import Store, Record, LAYER_BODY, LAYER_MIND, LAYER_JOURNAL

with Store("my_robot.flux", writable=True) as s:
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_BODY, kind="robot/link", meta={"name": "base"}))
        s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                          meta={"title": "抓取放置"},
                          payload=b"# 抓取物体,放到目标位置。"))
        s.put(txn, Record(layer=LAYER_JOURNAL, kind="signal",
                          meta={"name": "battery", "value": "12.4"}))
```

三种本性——身体 + 心智 + 日志——全在一个文件里。

## 投影到标准格式

```python
with Store("my_robot.flux") as s, s.read() as txn:
    s.to_usd(txn, "robot.usda")       # BODY -> USD
    s.to_okf(txn, "okf_out/")         # MIND -> OKF markdown
    s.to_mcap(txn, "run.mcap")        # JOURNAL -> MCAP
    s.to_fluxa(txn, "robot.fluxa")    # 全部 -> 文本(Git 友好)
```

## 导入已有机器人

```python
# 从 URDF(含关节限位 + 轴)
with Store("robot.flux", writable=True) as s, s.write() as txn:
    s.from_urdf(txn, "my_robot.urdf")

# 从 USD/SimReady
with Store("robot.flux", writable=True) as s, s.write() as txn:
    s.from_usd(txn, "scene.usda")
```

## CLI

```bash
flux inspect robot.flux              # 识别 + 统计
flux dump robot.flux                 # 列出所有记录
flux transcode robot.flux usd out.usda   # .flux -> USD
flux conv robot.flux robot.fluxa         # .flux -> .fluxa 文本
flux compose root.flux                    # 解析 LIVRPS 合并视图
flux from-simready scene.usda robot.flux  # SimReady -> DevReady
```

## 跑 demo

```bash
python demo/demo_newton.py              # USD <-> .flux + Newton 集成
python demo/demo_newton.py --newton     # 带 Newton 物理(需安装)
```
