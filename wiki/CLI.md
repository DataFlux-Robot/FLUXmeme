# CLI (`flux`)

The `flux` command-line tool is the `file` / `jq` / `usdcat` of the FLUXmeme
ecosystem — inspect, dump, transcode, convert, compose, and import.

Build from source: `cmake --build build --config Release` produces `build/bin/Release/flux`.

## Commands

### `flux inspect <file>`

Identify a `.flux` file and print stats:

```bash
$ flux inspect robot.flux
FLUXmeme v0.1.0
  path        : robot.flux
  commit_seq  : 1
  live records: 5
```

### `flux dump <file>`

List all records (id, layer, kind, path, payload preview):

```bash
$ flux dump robot.flux
019f0411ac9819354883672df8f5d3ec  BODY    robot/link   body/links/base
    "base link"
019f0411ac98f977fd9aa788bbca1775  MIND    concept
    "# Cart-pole balancing..."
```

### `flux transcode <file> <okf|a2a|usd> <out>`

Project a `.flux` to a standard format:

```bash
flux transcode robot.flux usd scene.usda     # BODY -> USDA
flux transcode robot.flux okf okf_out/        # MIND -> OKF markdown
flux transcode robot.flux a2a a2a_out/        # MIND -> A2A JSON
```

### `flux conv <in> <out>`

Convert between `.flux` (binary) and `.fluxa` (text canonical source). Direction
is detected by file extension:

```bash
flux conv robot.flux robot.fluxa    # binary -> text (for Git)
flux conv robot.fluxa robot.flux    # text -> binary
```

### `flux compose <root> [--variant S=V]...`

Resolve the LIVRPS merged view of a composition root:

```bash
flux compose root.flux                       # field-level merge
flux compose root.flux --variant config=heavy  # select a variant
```

### `flux from-simready <usda> <out.flux>`

Ingest a SimReady USD asset into a DevReady `.flux`:

```bash
flux from-simready scene.usda robot.flux
# SimReady -> DevReady: scene.usda -> robot.flux (5 records)
```

---

# CLI 工具(中文)

`flux` 是 FLUXmeme 生态的命令行工具——检查、转储、转录、转换、组合、导入。

## 命令

### `flux inspect <file>` — 识别 + 统计

### `flux dump <file>` — 列出所有记录

### `flux transcode <file> <okf|a2a|usd> <out>` — 投影到标准格式

```bash
flux transcode robot.flux usd scene.usda     # BODY -> USDA
flux transcode robot.flux okf okf_out/        # MIND -> OKF markdown
```

### `flux conv <in> <out>` — `.flux` <-> `.fluxa` 双形态转换

```bash
flux conv robot.flux robot.fluxa    # 二进制 -> 文本(Git 友好)
flux conv robot.fluxa robot.flux    # 文本 -> 二进制
```

### `flux compose <root> [--variant S=V]` — LIVRPS 组合视图

```bash
flux compose root.flux --variant config=heavy
```

### `flux from-simready <usda> <out.flux>` — SimReady -> DevReady 导入

```bash
flux from-simready scene.usda robot.flux
```
