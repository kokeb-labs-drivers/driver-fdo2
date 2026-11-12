"""Shared helpers for the FDO2 figures: read constants and enumerations from inc/ and src/.

Every helper raises when the code no longer looks the way a figure expects, so a figure fails to build instead of
drawing something the code does not do.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TYPES = "inc/fdo2_types.hpp"
IMPL = "src/fdo2.ipp"


def read(rel: str) -> str:
    return (ROOT / rel).read_text()


def need(pattern: str, text: str, what: str, flags: int = re.S) -> re.Match:
    m = re.search(pattern, text, flags)
    if not m:
        raise SystemExit(f"figure source drift: {what} not found (pattern {pattern!r})")
    return m


def enumerators(rel: str, name: str) -> list[str]:
    """The enumerator names of ``enum class <name>`` in order."""
    text = read(rel)
    body = need(rf"enum class (?:\[\[nodiscard\]\] )?{name} : \w+ \{{(.*?)\n\}};", text, f"enum {name}").group(1)
    return re.findall(r"^\s+(\w+)(?: = \d+)?,", body, re.M)


def config_default(field: str) -> int:
    """The default of a ``Config`` field, as a number."""
    text = read(TYPES)
    m = need(rf"\b\w+ {field}\{{([0-9']+)\}}", text, f"Config::{field}")
    return int(m.group(1).replace("'", ""))
