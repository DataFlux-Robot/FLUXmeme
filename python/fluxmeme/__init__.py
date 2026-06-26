"""FLUXmeme — self-describing asset format for embodied nodes.

One .flux = one embodied node = body + mind + lifetime journal. See SPEC.md.
"""
from .api import Store, Record, FluxError, LAYER_BODY, LAYER_MIND, LAYER_JOURNAL

__all__ = ["Store", "Record", "FluxError", "LAYER_BODY", "LAYER_MIND", "LAYER_JOURNAL"]

try:
    from ._version import __version__  # type: ignore
except ImportError:
    __version__ = "0.1.0"
