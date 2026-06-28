"""Low-level ctypes bindings to libfluxmeme. Mirrors include/fluxmeme/*.h.

NOTE: struct layout must match the C definitions EXACTLY (field order + types).
Validated by python/tests once CPython is installed. See README.
"""
from __future__ import annotations
import ctypes
import os
from ctypes import (
    Structure, POINTER, c_int, c_uint8, c_uint32, c_uint64, c_size_t,
    c_char_p, c_void_p, CDLL,
)

# ---- C structs (must match include/fluxmeme/types.h) -----------------------
# enums (flux_layer_t / flux_pclass_t / flux_clock_t / flux_status_t) are C
# `int` (4 bytes) under MSVC; use c_int, NOT c_uint8.

class flux_id_t(Structure):
    _fields_ = [("bytes", c_uint8 * 16)]

class flux_buf_t(Structure):
    _fields_ = [("data", POINTER(c_uint8)), ("len", c_size_t)]

class flux_meta_kv_t(Structure):
    _fields_ = [("key", c_char_p), ("val", c_char_p), ("type", c_uint8)]

# v2: no flux_link_t — connections are REF-typed meta entries

class flux_record_t(Structure):
    _fields_ = [
        ("id", flux_id_t),
        ("layer", c_int),
        ("pclass", c_int),
        ("clock", c_int),
        ("path", c_char_p),
        ("ptype", c_char_p),
        ("kind", c_char_p),
        ("meta", POINTER(flux_meta_kv_t)),
        ("meta_count", c_uint32),
        # v2: links/link_count DELETED — connections are REF meta
        ("payload", flux_buf_t),
        ("ts", c_uint64),
        ("ver", c_uint32),
    ]

# ---- enum mirrors ----------------------------------------------------------
LAYER_BODY, LAYER_MIND, LAYER_JOURNAL = 1, 2, 4
PCLASS_TEXT, PCLASS_BIN = 1, 2
CLOCK_SIM, CLOCK_WALL, CLOCK_DEVICE = 0, 1, 2
META_STRING, META_INT, META_FLOAT, META_BOOL, META_JSON, META_REF = 0, 1, 2, 3, 4, 5
OK = 0

# ---- load the shared library ----------------------------------------------
def _load() -> CDLL:
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(here, "fluxmeme.dll"),
        os.path.join(here, "libfluxmeme.so"),
        os.path.join(here, "libfluxmeme.dylib"),
    ]
    for p in candidates:
        if os.path.exists(p):
            return CDLL(p)
    try:
        return CDLL("fluxmeme")
    except OSError as e:
        raise RuntimeError(
            "libfluxmeme not found next to the package or on PATH. "
            "Build the C library first (see README)." ) from e

_lib = _load()

# ---- function signatures ---------------------------------------------------
_lib.fluxmeme_version.restype = c_char_p
_lib.flux_last_error.restype = c_char_p

_lib.flux_open.argtypes = [c_char_p, c_int, POINTER(c_void_p)]
_lib.flux_open.restype = c_int
_lib.flux_close.argtypes = [c_void_p]
_lib.flux_close.restype = c_int
_lib.flux_commit_seq.argtypes = [c_void_p]
_lib.flux_commit_seq.restype = c_uint64

_lib.flux_txn_begin_read.argtypes = [c_void_p, POINTER(c_void_p)]
_lib.flux_txn_begin_write.argtypes = [c_void_p, POINTER(c_void_p)]
_lib.flux_txn_commit.argtypes = [c_void_p]
_lib.flux_txn_rollback.argtypes = [c_void_p]

_lib.flux_put.argtypes = [c_void_p, POINTER(flux_record_t)]
_lib.flux_get.argtypes = [c_void_p, POINTER(flux_id_t), POINTER(flux_record_t)]
_lib.flux_del.argtypes = [c_void_p, POINTER(flux_id_t)]
_lib.flux_record_free.argtypes = [POINTER(flux_record_t)]

_lib.flux_scan.argtypes = [c_void_p, c_void_p, POINTER(c_void_p)]
_lib.flux_iter_next.argtypes = [c_void_p, POINTER(flux_record_t)]
_lib.flux_iter_free.argtypes = [c_void_p]

_lib.flux_to_okf.argtypes = [c_void_p, c_char_p]
_lib.flux_from_okf.argtypes = [c_char_p, c_void_p]
_lib.flux_to_a2a.argtypes = [c_void_p, c_char_p]
_lib.flux_from_a2a.argtypes = [c_char_p, c_void_p]
_lib.flux_to_usd.argtypes = [c_void_p, c_char_p]
_lib.flux_from_usd.argtypes = [c_char_p, c_void_p]
_lib.flux_conv_to_fluxa.argtypes = [c_void_p, c_char_p]
_lib.flux_conv_from_fluxa.argtypes = [c_char_p, c_void_p]
_lib.flux_to_mcap.argtypes = [c_void_p, c_char_p]
_lib.flux_from_mcap.argtypes = [c_char_p, c_void_p]
_lib.flux_from_urdf.argtypes = [c_char_p, c_void_p]
_lib.flux_from_sdf.argtypes = [c_char_p, c_void_p]
_lib.flux_to_mavlink.argtypes = [c_void_p, c_char_p]
_lib.flux_from_mavlink.argtypes = [c_char_p, c_void_p]
_lib.flux_del.argtypes = [c_void_p, POINTER(flux_id_t)]
_lib.flux_compact.argtypes = [c_char_p]


class FluxError(Exception):
    pass


def _check(rc: int, ctx: str = ""):
    if rc != OK:
        msg = _lib.flux_last_error().decode("utf-8", "replace") if _lib.flux_last_error() else ""
        raise FluxError(f"{ctx} failed (rc={rc}): {msg}")
