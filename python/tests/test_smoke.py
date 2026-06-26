"""Smoke test: put/get/scan + OKF round-trip from Python.
Requires CPython + a built libfluxmeme next to the package (pip install .)."""
import os
import tempfile

import pytest

import fluxmeme
from fluxmeme import Store, Record, LAYER_MIND


def test_put_get_scan():
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "t.flux")
        with Store(path, writable=True) as s:
            with s.write() as txn:
                s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                                  ptype="text/markdown",
                                  meta={"title": "A", "tags": "x"},
                                  payload=b"# A"))
                s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                                  meta={"title": "B"},
                                  payload=b"# B"))
            assert s.commit_seq == 1
            with s.read() as txn:
                recs = list(s.scan(txn, layer=LAYER_MIND, kind="concept"))
                titles = sorted(r.meta.get("title", "") for r in recs)
                assert titles == ["A", "B"]
                payloads = sorted(r.payload for r in recs)
                assert payloads == [b"# A", b"# B"]


def test_okf_roundtrip():
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "t.flux")
        okfdir = os.path.join(d, "okf_out")
        with Store(path, writable=True) as s:
            with s.write() as txn:
                s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                                  meta={"title": "Hello"},
                                  payload=b"# Hello"))
            with s.read() as txn:
                s.to_okf(txn, okfdir)
            assert os.path.isdir(okfdir)
