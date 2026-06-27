# FLUXmeme API Reference

Complete reference for the C and Python APIs. See also [SPEC.md](../SPEC.md) for
format semantics and [FORMAT.md](FORMAT.md) for the binary layout.

---

## C API (`include/fluxmeme/fluxmeme.h`)

### Types

```c
typedef enum { FLUX_LAYER_BODY=1, FLUX_LAYER_MIND=2, FLUX_LAYER_JOURNAL=4 } flux_layer_t;
typedef enum { FLUX_PCLASS_TEXT=1, FLUX_PCLASS_BIN=2 } flux_pclass_t;
typedef enum { FLUX_CLOCK_SIM_TIME=0, FLUX_CLOCK_WALL_TIME=1, FLUX_CLOCK_DEVICE_MONOTONIC=2 } flux_clock_t;
typedef enum { FLUX_OK, FLUX_ERR_IO, FLUX_ERR_CRC, FLUX_ERR_LOCKED,
               FLUX_ERR_NOTFOUND, FLUX_ERR_ARG, FLUX_ERR_VERSION, FLUX_ERR_NOMEM,
               FLUX_ERR_RANGE, FLUX_ERR_CORRUPT } flux_status_t;

typedef struct { uint8_t bytes[16]; } flux_id_t;
typedef struct { const uint8_t* data; size_t len; } flux_buf_t;
typedef struct { const char* key; const char* val; } flux_meta_kv_t;
typedef struct { flux_id_t target; const char* rel; } flux_link_t;

typedef struct {
    flux_id_t id; flux_layer_t layer; flux_pclass_t pclass; flux_clock_t clock;
    const char* path, *ptype, *kind;
    const flux_meta_kv_t* meta; uint32_t meta_count;
    const flux_link_t* links; uint32_t link_count;
    flux_buf_t payload; uint64_t ts; uint32_t ver;
} flux_record_t;
```

### Lifecycle

```c
flux_status_t flux_open(const char* path, int writable, flux_store_t** out);
flux_status_t flux_open_with_backend(flux_backend_t* be, flux_store_t** out);
void          flux_close(flux_store_t* store);
uint64_t      flux_commit_seq(const flux_store_t* store);
flux_status_t flux_compact(const char* path);           // reclaim append growth
```

### Transactions (MVCC)

```c
flux_status_t flux_txn_begin_read (flux_store_t* s, flux_txn_t** out);  // lock-free snapshot
flux_status_t flux_txn_begin_write(flux_store_t* s, flux_txn_t** out);  // exclusive
flux_status_t flux_txn_commit     (flux_txn_t* txn);
flux_status_t flux_txn_rollback   (flux_txn_t* txn);
```

Read transactions never block writers; writers never block readers. A read txn
captures `commit_seq` atomically at begin time and sees all records with
`ver <= snapshot` minus live tombstones.

### CRUD

```c
flux_status_t flux_put(flux_txn_t* txn, flux_record_t* rec);            // fills rec->id/ver
flux_status_t flux_get(flux_txn_t* txn, const flux_id_t* id, flux_record_t* out);
flux_status_t flux_del(flux_txn_t* txn, const flux_id_t* id);            // tombstone
flux_status_t flux_cas(flux_txn_t* txn, const flux_id_t* id,             // optimistic CAS
                       uint32_t expected_ver, flux_record_t* newrec);
void flux_record_free(flux_record_t* rec);
```

**Ownership:** input pointers to `flux_put` are borrowed (engine copies).
Records returned from `flux_get`/`flux_iter_next` are heap copies — free with
`flux_record_free`.

### Scan / filter

```c
typedef struct {
    uint32_t layer_mask; const char* kind; const char* path_prefix;
    uint64_t ts_lo, ts_hi;
} flux_filter_t;

flux_status_t flux_scan(const flux_txn_t* txn, const flux_filter_t* filter, flux_iter_t** out);
flux_status_t flux_iter_next(flux_iter_t* it, flux_record_t* out);  // FLUX_ERR_NOTFOUND when done
void          flux_iter_free(flux_iter_t* it);
```

Pass `NULL` as `filter` to scan all live records. `layer_mask` is a bitset
(`FLUX_LAYER_BODY | FLUX_LAYER_MIND` matches records in either layer).

### FIFO cursors (multi-consumer streaming)

```c
flux_status_t flux_cursor_open(flux_store_t* s, uint64_t start_seq, flux_cursor_t** out);
flux_status_t flux_cursor_next(flux_cursor_t* c, flux_record_t* out);
uint64_t      flux_cursor_seq(const flux_cursor_t* c);
void          flux_cursor_free(flux_cursor_t* c);
```

Each cursor has independent progress over the append-only log. Multiple
consumers can tail the same store without interfering (MIMO).

### Transcoders (projections to/from standard formats)

```c
// USD (BODY layer)
flux_status_t flux_to_usd  (const flux_txn_t* txn, const char* out_usda);
flux_status_t flux_from_usd(const char* in_usda, flux_txn_t* txn);

// OKF (MIND layer, kind=concept)
flux_status_t flux_to_okf  (const flux_txn_t* txn, const char* out_dir);
flux_status_t flux_from_okf(const char* in_dir, flux_txn_t* txn);

// A2A (MIND layer, kind=agent_card|task)
flux_status_t flux_to_a2a  (const flux_txn_t* txn, const char* out_dir);
flux_status_t flux_from_a2a(const char* in_dir, flux_txn_t* txn);

// MCAP (JOURNAL layer, kind=signal) -- rosbag2
flux_status_t flux_to_mcap  (const flux_txn_t* txn, const char* out_mcap);
flux_status_t flux_from_mcap(const char* in_mcap, flux_txn_t* txn);

// MAVLink (JOURNAL layer, kind=signal) -- frame transport
flux_status_t flux_to_mavlink  (const flux_txn_t* txn, const char* out_frames);
flux_status_t flux_from_mavlink(const char* in_frames, flux_txn_t* txn);

// .fluxa (whole store, text canonical source)
flux_status_t flux_conv_to_fluxa  (const flux_txn_t* txn, const char* out_fluxa);
flux_status_t flux_conv_from_fluxa(const char* in_fluxa, flux_txn_t* txn);

// Robot description import (XML -> BODY robot-graph)
flux_status_t flux_from_urdf(const char* in_urdf, flux_txn_t* txn);
flux_status_t flux_from_sdf (const char* in_sdf,  flux_txn_t* txn);
```

### Composition (LIVRPS)

```c
// src/compose/compose.h
flux_status_t flux_compose_open(const char* root_path, flux_compose_t** out);
flux_status_t flux_compose_set_variant(flux_compose_t* c, const char* set, const char* value);
flux_status_t flux_compose_get(flux_compose_t* c, const flux_id_t* id, flux_record_t* out);

flux_status_t flux_compose_scan(flux_compose_t* c, const flux_filter_t* f, flux_compose_iter_t** out);
flux_status_t flux_compose_iter_next(flux_compose_iter_t* it, flux_record_t* out);
void          flux_compose_iter_free(flux_compose_iter_t* it);

void flux_compose_close(flux_compose_t* c);
size_t flux_compose_n_layers(const flux_compose_t* c);
```

### Robot facets

```c
// src/facets/robot_graph.h — closed-loop kinematics
typedef struct { flux_id_t id; char name[64]; } flux_rlink;
typedef struct { flux_id_t id; flux_id_t parent; flux_id_t child; char type[16]; } flux_rjoint;
typedef struct { flux_rlink* links; size_t n_links; flux_rjoint* joints; size_t n_joints; } flux_robot_graph;

flux_status_t flux_robot_load(const flux_txn_t* txn, flux_robot_graph* out);
void flux_robot_graph_free(flux_robot_graph* g);
int flux_robot_has_cycle(const flux_robot_graph* g);  // 1 = closed loop

// src/facets/device_comm.h — device/bus topology
typedef struct { flux_id_t id; char name[64]; char kind[8]; char protocol[16]; char addr[32]; uint64_t baud; } flux_dnode;
typedef struct { flux_id_t id; flux_id_t a; flux_id_t b; char protocol[16]; } flux_dedge;
typedef struct { flux_dnode* nodes; size_t n_nodes; flux_dedge* edges; size_t n_edges; } flux_dcomm;

flux_status_t flux_dcomm_load(const flux_txn_t* txn, flux_dcomm* out);
void flux_dcomm_free(flux_dcomm* g);
size_t flux_dcomm_edges_by_protocol(const flux_dcomm* g, const char* protocol, const flux_dedge** out);
```

### Backend abstraction

```c
// include/fluxmeme/backend.h
typedef struct flux_backend {
    flux_status_t (*open)(void* self, const char* path, int writable);
    int64_t (*read)(void* self, uint64_t off, void* buf, size_t n);
    int64_t (*append)(void* self, const void* buf, size_t n);
    flux_status_t (*sync)(void* self);
    flux_status_t (*lock)(void* self, int exclusive);
    flux_status_t (*unlock)(void* self);
    flux_status_t (*size)(void* self, uint64_t* out);
    flux_status_t (*close)(void* self);
    void* self;
} flux_backend_t;

flux_backend_t* flux_backend_file(void);  // shipped portable backend
```

### Codec registry

```c
// include/fluxmeme/codec.h
typedef flux_status_t (*flux_encode_fn)(const flux_txn_t* txn, const char* out);
typedef flux_status_t (*flux_decode_fn)(const char* in, flux_txn_t* txn);
typedef struct { const char* name; flux_encode_fn encode; flux_decode_fn decode; } flux_codec_t;

flux_status_t flux_codec_register(const flux_codec_t* codec);
const flux_codec_t* flux_codec_find(const char* name);
```

---

## Python API (`fluxmeme`)

Install: `pip install git+https://github.com/DataFlux-Robot/FLUXmeme.git`

```python
from fluxmeme import Store, Record, FluxError
from fluxmeme import LAYER_BODY, LAYER_MIND, LAYER_JOURNAL
from fluxmeme import PCLASS_TEXT, PCLASS_BIN
```

### `Store` — the main handle

```python
store = Store(path: str, writable: bool = True)
store.close()
store.commit_seq -> int

# Context manager
with Store("robot.flux", writable=True) as s:
    ...

# Transactions (context managers)
with s.read() as txn:       # lock-free snapshot read
    ...
with s.write() as txn:      # exclusive write (auto-commit / auto-rollback)
    ...

# CRUD
s.put(txn, record: Record) -> Record     # fills record.id and record.ver
s.get(txn, rid: str) -> Record           # by 32-hex id
s.delete(txn, rid: str)                  # tombstone
s.scan(txn, *, layer=None, kind=None) -> Iterator[Record]

# Transcoders (all operate on a read or write txn)
s.to_usd(txn, out_usda: str)
s.from_usd(txn, in_usda: str)
s.to_okf(txn, out_dir: str)
s.from_okf(txn, in_dir: str)
s.to_a2a(txn, out_dir: str)
s.from_a2a(txn, in_dir: str)
s.to_mcap(txn, out: str)
s.from_mcap(txn, inp: str)
s.to_mavlink(txn, out: str)
s.from_mavlink(txn, inp: str)
s.to_fluxa(txn, out: str)
s.from_fluxa(txn, inp: str)
s.from_urdf(txn, inp: str)
s.from_sdf(txn, inp: str)

# Reclaim growth
Store.compact(path: str)  # static method
```

### `Record` — the universal atom

```python
@dataclass
class Record:
    layer: int = LAYER_MIND          # LAYER_BODY=1 | LAYER_MIND=2 | LAYER_JOURNAL=4
    kind: str | None = None          # "concept", "robot/link", "signal", ...
    ptype: str | None = None         # MIME hint: "text/markdown", "application/json", ...
    path: str | None = None          # in-layer human-readable path
    pclass: int = PCLASS_TEXT        # PCLASS_TEXT=1 | PCLASS_BIN=2
    clock: int = 0                   # for JOURNAL: CLOCK_SIM/WALL/DEVICE
    payload: bytes = b""             # TEXT (UTF-8) or BIN
    meta: dict = {}                  # OKF-style frontmatter key/value
    links: list[tuple[str, str]] = []  # (target_hex_32, rel)
    id: str | None = None            # 32-hex ULID; set by put()
    ts: int = 0                      # timestamp
    ver: int = 0                     # MVCC version; set by put()/get()
```

**Per-layer usage:**

| Layer | Typical `kind` | `payload` | `meta` keys |
|---|---|---|---|
| BODY | `robot/link`, `robot/joint`, `mesh` | geometry bytes or empty | `name`, `type`, `mass`, `lower`, `upper`, `axis` |
| MIND | `concept`, `agent_card`, `task` | markdown body or JSON | `title`, `tags`, `type` |
| JOURNAL | `signal`, `param`, `phm_slice` | sample data | `name`, `value` |

### Error handling

```python
from fluxmeme import FluxError

try:
    with Store("robot.flux") as s:
        s.get(s.read().__enter__(), "nonexistent_id")
except FluxError as e:
    print(f"FLUXmeme error: {e}")
```

`FluxError` is raised on any non-OK C status. The message includes the
thread-local error string from `flux_last_error()`.
