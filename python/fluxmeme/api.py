"""High-level Pythonic API for FLUXmeme. See README / SPEC.md."""
from __future__ import annotations
import ctypes
from contextlib import contextmanager
from dataclasses import dataclass, field
from typing import Iterator, List, Optional, Tuple

from . import _core
from ._core import (
    flux_record_t, flux_meta_kv_t, flux_id_t, flux_buf_t,
    LAYER_BODY, LAYER_MIND, LAYER_JOURNAL, PCLASS_TEXT, FluxError,
    META_STRING, META_INT, META_FLOAT, META_BOOL, META_JSON, META_REF,
)

LAYER_NAMES = {LAYER_BODY: "BODY", LAYER_MIND: "MIND", LAYER_JOURNAL: "JOURNAL"}
NAME_TO_LAYER = {v: k for k, v in LAYER_NAMES.items()}


def _id_to_hex(b: bytes) -> str:
    return b.hex()


def _hex_to_id(s: str) -> bytes:
    return bytes.fromhex(s)


@dataclass
class Record:
    layer: int = LAYER_MIND
    kind: Optional[str] = None
    ptype: Optional[str] = None
    path: Optional[str] = None
    pclass: int = PCLASS_TEXT
    clock: int = 0
    payload: bytes = b""
    meta: dict = field(default_factory=dict)          # key -> value (all strings)
    meta_types: dict = field(default_factory=dict)    # key -> META_* type (default STRING)
    refs: List[Tuple[str, str, str]] = field(default_factory=list)  # (rel, target_hex, graph)
    id: Optional[str] = None
    ts: int = 0
    ver: int = 0


class Store:
    """A FLUXmeme store handle. Use as a context manager."""

    def __init__(self, path: str, writable: bool = True):
        self._p = ctypes.c_void_p()
        _core._check(_core._lib.flux_open(path.encode(), 1 if writable else 0,
                                          ctypes.byref(self._p)), "open")
        self._closed = False

    def close(self):
        if not self._closed:
            _core._lib.flux_close(self._p)
            self._closed = True

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    @property
    def commit_seq(self) -> int:
        return _core._lib.flux_commit_seq(self._p)

    # ---- transactions ----
    @contextmanager
    def read(self):
        t = ctypes.c_void_p()
        _core._check(_core._lib.flux_txn_begin_read(self._p, ctypes.byref(t)), "begin_read")
        try:
            yield t
        finally:
            # read txns are freed by commit/rollback; call rollback to free
            _core._lib.flux_txn_rollback(t)

    @contextmanager
    def write(self):
        t = ctypes.c_void_p()
        _core._check(_core._lib.flux_txn_begin_write(self._p, ctypes.byref(t)), "begin_write")
        try:
            yield t
            _core._check(_core._lib.flux_txn_commit(t), "commit")
        except Exception:
            _core._lib.flux_txn_rollback(t)
            raise

    # ---- CRUD ----
    def put(self, txn, rec: Record) -> Record:
        crec, _keep = _record_to_c(rec)
        _core._check(_core._lib.flux_put(txn, ctypes.byref(crec)), "put")
        rec.id = _id_to_hex(bytes(crec.id.bytes))
        rec.ver = crec.ver
        return rec

    def get(self, txn, rid: str) -> Record:
        cid = flux_id_t()
        cid.bytes = (ctypes.c_uint8 * 16)(*_hex_to_id(rid))
        crec = flux_record_t()
        _core._check(_core._lib.flux_get(txn, ctypes.byref(cid), ctypes.byref(crec)), "get")
        try:
            return _record_from_c(crec)
        finally:
            _core._lib.flux_record_free(ctypes.byref(crec))

    def scan(self, txn, *, layer: Optional[int] = None,
             kind: Optional[str] = None) -> Iterator[Record]:
        it = ctypes.c_void_p()
        _core._check(_core._lib.flux_scan(txn, None, ctypes.byref(it)), "scan")
        try:
            crec = flux_record_t()
            while _core._lib.flux_iter_next(it, ctypes.byref(crec)) == _core.OK:
                try:
                    rec = _record_from_c(crec)
                finally:
                    _core._lib.flux_record_free(ctypes.byref(crec))
                if layer is not None and not (rec.layer & layer):
                    continue
                if kind is not None and rec.kind != kind:
                    continue
                yield rec
        finally:
            _core._lib.flux_iter_free(it)

    # ---- transcoders ----
    def to_okf(self, txn, out_dir: str): _core._check(_core._lib.flux_to_okf(txn, out_dir.encode()), "to_okf")
    def from_okf(self, txn, in_dir: str): _core._check(_core._lib.flux_from_okf(in_dir.encode(), txn), "from_okf")
    def to_a2a(self, txn, out_dir: str): _core._check(_core._lib.flux_to_a2a(txn, out_dir.encode()), "to_a2a")
    def from_a2a(self, txn, in_dir: str): _core._check(_core._lib.flux_from_a2a(in_dir.encode(), txn), "from_a2a")
    def to_usd(self, txn, out_usda: str): _core._check(_core._lib.flux_to_usd(txn, out_usda.encode()), "to_usd")
    def from_usd(self, txn, in_usda: str): _core._check(_core._lib.flux_from_usd(in_usda.encode(), txn), "from_usd")
    def to_fluxa(self, txn, out: str): _core._check(_core._lib.flux_conv_to_fluxa(txn, out.encode()), "to_fluxa")
    def from_fluxa(self, txn, inp: str): _core._check(_core._lib.flux_conv_from_fluxa(inp.encode(), txn), "from_fluxa")
    def to_mcap(self, txn, out: str): _core._check(_core._lib.flux_to_mcap(txn, out.encode()), "to_mcap")
    def from_mcap(self, txn, inp: str): _core._check(_core._lib.flux_from_mcap(inp.encode(), txn), "from_mcap")
    def from_urdf(self, txn, inp: str): _core._check(_core._lib.flux_from_urdf(inp.encode(), txn), "from_urdf")
    def from_sdf(self, txn, inp: str): _core._check(_core._lib.flux_from_sdf(inp.encode(), txn), "from_sdf")
    def to_mavlink(self, txn, out: str): _core._check(_core._lib.flux_to_mavlink(txn, out.encode()), "to_mavlink")
    def from_mavlink(self, txn, inp: str): _core._check(_core._lib.flux_from_mavlink(inp.encode(), txn), "from_mavlink")

    def delete(self, txn, rid: str):
        """Tombstone a record by hex id."""
        cid = _core.flux_id_t()
        cid.bytes = (ctypes.c_uint8 * 16)(*_hex_to_id(rid))
        _core._check(_core._lib.flux_del(txn, ctypes.byref(cid)), "delete")

    @staticmethod
    def compact(path: str):
        """Reclaim append-only growth (drop tombstones + superseded versions)."""
        _core._check(_core._lib.flux_compact(path.encode()), "compact")


# ---- marshaling ------------------------------------------------------------
def _record_to_c(rec: Record):
    keep = []  # hold refs so ctypes pointers stay valid during the call
    c = flux_record_t()
    if rec.id:
        c.id.bytes = (ctypes.c_uint8 * 16)(*_hex_to_id(rec.id))
    c.layer = int(rec.layer)
    c.pclass = int(rec.pclass)
    c.clock = int(rec.clock)
    c.path = rec.path.encode() if rec.path else None
    c.ptype = rec.ptype.encode() if rec.ptype else None
    c.kind = rec.kind.encode() if rec.kind else None

    if rec.meta:
        # combine plain meta + refs into one meta array
        all_meta = []
        for k, v in rec.meta.items():
            t = rec.meta_types.get(k, META_STRING)
            all_meta.append((k, str(v), t))
        for rel, tgt_hex, graph in rec.refs:
            val = f"{tgt_hex}@{graph}" if graph else tgt_hex
            all_meta.append((rel, val, META_REF))
        if all_meta:
            arr = (flux_meta_kv_t * len(all_meta))()
            for i, (k, v, t) in enumerate(all_meta):
                arr[i].key = str(k).encode()
                arr[i].val = str(v).encode()
                arr[i].type = t
            c.meta = ctypes.cast(arr, ctypes.POINTER(flux_meta_kv_t))
            c.meta_count = len(all_meta)
            keep.append(arr)
    if rec.payload:
        buf = (ctypes.c_uint8 * len(rec.payload))(*rec.payload)
        c.payload.data = ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8))
        c.payload.len = len(rec.payload)
        keep.append(buf)
    c.ts = int(rec.ts)
    return c, keep


def _record_from_c(c: flux_record_t) -> Record:
    rec = Record()
    rec.id = _id_to_hex(bytes(c.id.bytes))
    rec.layer = int(c.layer)
    rec.pclass = int(c.pclass)
    rec.clock = int(c.clock)
    rec.path = c.path.decode("utf-8", "replace") if c.path else None
    rec.ptype = c.ptype.decode("utf-8", "replace") if c.ptype else None
    rec.kind = c.kind.decode("utf-8", "replace") if c.kind else None
    rec.ts = int(c.ts)
    rec.ver = int(c.ver)
    meta = {}
    meta_types = {}
    refs = []
    for i in range(c.meta_count):
        kv = c.meta[i]
        k = kv.key.decode("utf-8", "replace") if kv.key else ""
        v = kv.val.decode("utf-8", "replace") if kv.val else ""
        t = int(kv.type)
        if t == META_REF:
            # parse "hex@graph"
            if "@" in v:
                hex_id, graph = v.split("@", 1)
            else:
                hex_id, graph = v, ""
            refs.append((k, hex_id, graph))
        else:
            meta[k] = v
            meta_types[k] = t
    rec.meta = meta
    rec.meta_types = meta_types
    rec.refs = refs
    if c.payload.len:
        rec.payload = bytes(ctypes.string_at(c.payload.data, c.payload.len))
    else:
        rec.payload = b""
    return rec
