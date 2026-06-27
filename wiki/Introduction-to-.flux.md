# Introduction to .flux

## What is a `.flux` file?

A `.flux` file is a **self-describing, append-only binary store** that holds one
embodied node (a robot) through its entire lifecycle. It is:

- **One file** — body + mind + journal, no sidecar drift
- **Self-describing** — magic `FLXM` + schema_id; `flux inspect` identifies any file
- **Byte-portable** — identical on cloud GPU and Cortex-M7 MCU
- **Append-only** — the journal grows forever; the body/mind stay constant
- **MVCC** — lock-free multi-reader; single serialized writer; crash-safe

## Inside a `.flux`

```
[FileHeader  64 B]   magic "FLXM" | version | schema_id | crc
[RecordEntry]*       "RECD" + body_len + Record + crc    (append-only log)
[CommitMarker]*      "COMT" + commit_seq + ts + delta    (per transaction)
```

Each `RecordEntry` is a **Record** — the universal atom:

```
id(16B ULID) | layer(BODY/MIND/JOURNAL) | kind | meta(KV) | links(edges)
              | payload(TEXT/BIN) | ts + clock | ver(MVCC)
```

### Records are dumb on purpose

The format stores **bytes + structured metadata**. It does NOT:
- Parse mesh geometry (that's the USD transcoder's job)
- Solve FK/IK (that's the sim engine's job)
- Execute code (no runtime, no scripts)
- Understand "robot" or "task" semantics

It just stores and retrieves records. Adding a new domain = a new `kind`, never
an engine change.

## The three layers

```
┌─────────────────────────────────────────┐
│              .flux file                  │
│                                         │
│  BODY    → USD        structure + mesh  │
│  MIND    → OKF + A2A  knowledge + agent │
│  JOURNAL → MCAP + MAV telemetry + PHM   │
│                                         │
└─────────────────────────────────────────┘
```

Each layer is a **bitset** on the `layer` field. A record can belong to one or
more layers. Transcoders scan by layer and project to the target format.

## Dual form: `.fluxa` (text)

Every `.flux` has a text twin: **`.fluxa`** — the canonical source. It is:

- Human-readable (line-oriented: `R <id>` blocks + tag lines)
- Diff-friendly (Git PRs, code review)
- Losslessly interconvertible (`flux conv`)

```
#FLUXMEME 1.0
R 019f0411ac9819354883672df8f5d3ec
L BODY
K mesh
P model/stl
B BIN
X 8
00ff1122
R 019f0411ac98f977fd9aa788bbca1775
L MIND
K concept
M title=Hello
D 7
# Hello
```

## How reads/writes work

- **Write**: open a write txn (exclusive lock) → `flux_put(record)` → commit
  (append records + CommitMarker + fsync). Crash-safe: uncommitted tail is
  truncated on reopen.
- **Read**: open a read txn (lock-free snapshot at a `commit_seq`) → `flux_get(id)`
  or `flux_scan(filter)`. Multiple readers never block each other or the writer.
- **Compact**: `flux_compact(path)` rewrites the file keeping only live records
  (drops tombstones + superseded versions).

## Projections (transcoders)

A `.flux` is the **single source**. Transcoders project to standard formats:

| Transcoder | Layer | Output |
|---|---|---|
| `to_usd` | BODY | USDA scene (mesh: points/indices/normals) |
| `to_okf` | MIND | OKF markdown bundle (concepts + frontmatter) |
| `to_a2a` | MIND | A2A JSON (agent-card.json + tasks/) |
| `to_mcap` | JOURNAL | MCAP rosbag2 (signals as messages) |
| `to_mavlink` | JOURNAL | MAVLink v2 frames (signals only; mesh filtered) |
| `to_fluxa` | whole | .fluxa text (canonical source) |

Each has a `from_*` reverse. Your tools consume the standard they already speak.

---

# .flux 入门(中文)

## `.flux` 文件是什么?

一个 `.flux` 文件是**自描述、append-only 二进制存储**,贯穿一个具身节点(机器人)
的整个生命周期。它是:

- **一个文件** — 身体 + 心智 + 日志,无 sidecar 漂移
- **自描述** — magic `FLXM` + schema_id;`flux inspect` 识别任意文件
- **字节可移植** — 云 GPU 和 Cortex-M7 MCU 上字节相同
- **Append-only** — 日志永远生长;身体/心智保持不变
- **MVCC** — 无锁多读者;单写者串行;崩溃安全

## 内部结构

```
[FileHeader  64B]   magic "FLXM" | 版本 | schema_id | crc
[RecordEntry]*      "RECD" + body_len + Record + crc    (追加日志)
[CommitMarker]*      "COMT" + commit_seq + ts + delta    (每事务)
```

每条 `RecordEntry` 是一个 **Record** — 万物原子:

```
id(16B ULID) | layer(BODY/MIND/JOURNAL) | kind | meta(KV) | links(边)
              | payload(TEXT/BIN) | ts + clock | ver(MVCC)
```

### Record 故意是"哑"的

格式只存**字节 + 结构化元数据**。它不:
- 解析 mesh 几何(那是 USD transcoder 的事)
- 求解 FK/IK(那是仿真引擎的事)
- 执行代码(无运行时、无脚本)
- 理解"机器人"或"任务"语义

它只存取记录。加新领域 = 加一个 `kind`,不改引擎。

## 双形态:`.fluxa`(文本)

每个 `.flux` 有一个文本孪生:**`.fluxa`** — canonical 源。它是:
- 人可读(行式:`R <id>` 块 + 标签行)
- 可 diff(Git PR、代码审查)
- 无损互转(`flux conv`)

## 读写怎么工作

- **写**:开写事务(独占锁)→ `flux_put(record)` → 提交(追加记录 + CommitMarker + fsync)
- **读**:开读事务(无锁快照)→ `flux_get(id)` 或 `flux_scan(filter)`。多读者互不阻塞
- **压缩**:`flux_compact(path)` 重写文件只保留存活记录

## 投影(transcoder)

`.flux` 是**单一源**。Transcoder 投影到标准格式:

| Transcoder | 层 | 输出 |
|---|---|---|
| `to_usd` | BODY | USDA 场景(mesh) |
| `to_okf` | MIND | OKF markdown 概念包 |
| `to_a2a` | MIND | A2A JSON(agent-card.json) |
| `to_mcap` | JOURNAL | MCAP rosbag2(信号) |
| `to_mavlink` | JOURNAL | MAVLink v2 帧(仅信号) |
| `to_fluxa` | 全部 | .fluxa 文本(canonical 源) |

每个都有 `from_*` 反向。你的工具消费它已经说的标准。
