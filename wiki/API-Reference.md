# API Reference

Complete C and Python API. See also [docs/API.md](https://github.com/DataFlux-Robot/FLUXmeme/blob/main/docs/API.md)
for the full reference with every type and method.

## Python (recommended)

```python
from fluxmeme import Store, Record, FluxError
from fluxmeme import LAYER_BODY, LAYER_MIND, LAYER_JOURNAL
```

### Store

```python
Store(path, writable=True)        # open / create
Store.compact(path)               # reclaim growth (static)
s.commit_seq -> int               # current MVCC version
s.close()

with s.read() as txn:             # lock-free snapshot read
with s.write() as txn:            # exclusive write (auto-commit/rollback)

s.put(txn, record) -> Record      # create / update (fills id + ver)
s.get(txn, rid: str) -> Record    # read by 32-hex id
s.delete(txn, rid: str)           # tombstone
s.scan(txn, *, layer=None, kind=None) -> Iterator[Record]

# transcoders (5 projections + 2 robot imports + dual form)
s.to_usd(txn, out)        s.from_usd(txn, in_)
s.to_okf(txn, out)        s.from_okf(txn, in_)
s.to_a2a(txn, out)        s.from_a2a(txn, in_)
s.to_mcap(txn, out)       s.from_mcap(txn, in_)
s.to_mavlink(txn, out)    s.from_mavlink(txn, in_)
s.to_fluxa(txn, out)      s.from_fluxa(txn, in_)
                          s.from_urdf(txn, in_)
                          s.from_sdf(txn, in_)
```

### Record

```python
Record(
    layer=LAYER_MIND,           # BODY=1 | MIND=2 | JOURNAL=4
    kind="concept",             # canonical or open
    payload=b"# markdown",      # TEXT (UTF-8) or BIN
    meta={"title": "Task"},     # OKF-style frontmatter
    links=[(target_hex, rel)],  # directed graph edges
    ptype="text/markdown",      # MIME hint
    path="mind/concepts/task",  # human-readable path
    pclass=PCLASS_TEXT,         # TEXT=1 | BIN=2
    clock=0,                    # for JOURNAL: sim/wall/device
    id=None,                    # set by put()
    ts=0, ver=0,                # set by put()/get()
)
```

## C (`include/fluxmeme/fluxmeme.h`)

```c
flux_open / flux_close / flux_compact
flux_txn_begin_read / begin_write / commit / rollback
flux_put / flux_get / flux_del / flux_cas
flux_scan / flux_iter_next / flux_iter_free
flux_cursor_open / next / free
flux_to/from_{usd,okf,a2a,mcap,mavlink}
flux_conv_to/from_fluxa
flux_from_urdf / flux_from_sdf
```

Composition (`src/compose/compose.h`):
```c
flux_compose_open / close / n_layers
flux_compose_set_variant
flux_compose_get / scan / iter_next
```

Robot facets (`src/facets/`):
```c
flux_robot_load / has_cycle / graph_free
flux_dcomm_load / edges_by_protocol / free
```

---

# API 参考(中文)

完整 C 和 Python API。另见 [docs/API.md](https://github.com/DataFlux-Robot/FLUXmeme/blob/main/docs/API.md)。

## Python(推荐)

```python
from fluxmeme import Store, Record, LAYER_BODY, LAYER_MIND, LAYER_JOURNAL

# 打开/创建
with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_BODY, kind="robot/link", meta={"name": "base"}))
    with s.read() as txn:
        recs = list(s.scan(txn, layer=LAYER_BODY))
        s.to_usd(txn, "scene.usda")    # BODY -> USD

# 5 个 transcoder:
s.to_usd / to_okf / to_a2a / to_mcap / to_mavlink
# 导入:
s.from_usd / from_urdf / from_sdf
# 双形态:
s.to_fluxa / from_fluxa
```

## C

```c
flux_open / close / compact
flux_txn_begin_read / begin_write / commit / rollback
flux_put / get / del / cas / scan / iter / cursor
flux_to/from_{usd,okf,a2a,mcap,mavlink} / conv_to/from_fluxa / from_urdf/sdf
flux_compose_open / set_variant / get / scan
flux_robot_load / has_cycle / dcomm_load / edges_by_protocol
```
