"""Subset of future.utils needed by the ESP-IDF 4.4 coredump decoder."""


def with_metaclass(meta, *bases):
    """Create a temporary base class with ``meta`` as its metaclass."""
    return meta("TemporaryClass", bases, {})
