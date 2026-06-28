"""FLUXmeme — self-describing asset format for embodied nodes.

One .flux = one embodied node = body + mind + lifetime journal. See SPEC.md.
"""
from .api import Store, Record, FluxError
from ._core import (
    LAYER_BODY, LAYER_MIND, LAYER_JOURNAL,
    PCLASS_TEXT, PCLASS_BIN,
    META_STRING, META_INT, META_FLOAT, META_BOOL, META_JSON, META_REF,
)

__all__ = [
    "Store", "Record", "FluxError",
    "LAYER_BODY", "LAYER_MIND", "LAYER_JOURNAL",
    "PCLASS_TEXT", "PCLASS_BIN",
    "META_STRING", "META_INT", "META_FLOAT", "META_BOOL", "META_JSON", "META_REF",
]

try:
    from ._version import __version__  # type: ignore
except ImportError:
    __version__ = "0.1.0"
