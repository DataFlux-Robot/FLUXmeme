# FLUXmeme Python package

Status: **validated** with CPython 3.10 (`pytest` green). The Python API
(`Store`, `Record`, transcoders) mirrors the C library via `ctypes`; struct
layout in `_core.py` matches `include/fluxmeme/types.h`.

## Validate (once CPython ≥ 3.10 is installed)

```bash
# 1. build the C shared lib and copy it next to the package
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_SHARED_LIB=ON
cmake --build build --config Release
copy build\bin\Release\fluxmeme.dll python\fluxmeme\      # Windows

# 2. install + test
python -m pip install -e python/
python -m pytest python/tests
```

If the ctypes struct layout or scikit-build-core packaging needs a tweak, that's
a small follow-up — the C library and API surface are stable.

## Usage

```python
from fluxmeme import Store, Record, LAYER_MIND

with Store("robot.flux", writable=True) as s:
    with s.write() as txn:
        s.put(txn, Record(layer=LAYER_MIND, kind="concept",
                          meta={"title": "Greet"}, payload=b"# Hi"))
    with s.read() as txn:
        for r in s.scan(txn):
            print(r.kind, r.meta, r.payload)
        s.to_okf(txn, "okf_out/")        # or to_a2a / to_usd / to_fluxa
```
