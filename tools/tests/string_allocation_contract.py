#!/usr/bin/env python3
"""Guard idStr allocation arithmetic before legacy int narrowing."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def require(source: str, needle: str, context: str) -> None:
    if needle not in source:
        raise AssertionError(f"Missing {needle!r} in {context}")


def reject(source: str, needle: str, context: str) -> None:
    if needle in source:
        raise AssertionError(f"Unexpected overflow-prone expression {needle!r} in {context}")


def main() -> None:
    header = (ROOT / "src/idlib/Str.h").read_text(encoding="utf-8")
    source = (ROOT / "src/idlib/Str.cpp").read_text(
        encoding="windows-1252", errors="surrogateescape"
    )
    allocation = (ROOT / "src/idlib/StrAllocation.h").read_text(encoding="utf-8")
    probe = (ROOT / "tools/tests/string_allocation_probe.cpp").read_text(encoding="utf-8")

    require(header, "ReAllocate( size_t amount", "idStr allocation API")
    require(header, "EnsureAlloced( size_t amount", "idStr allocation API")
    require(
        header,
        "amount > static_cast<size_t>( alloced )",
        "idStr allocation-width comparison",
    )
    require(source, "TryRoundUpToInt( amount, STR_ALLOC_GRAN, newsize )", "idStr::ReAllocate")
    require(source, "oldLen == 0", "idStr::Replace empty-pattern guard")
    require(source, "SaturatingMultiply", "idStr::Replace result sizing")
    require(header, 'idLib::Error( "idStr::Append: negative length" )', "bounded Append")
    require(header, 'idLib::Error( "idStr::Fill: negative length" )', "bounded Fill")
    if header.count("if ( end <= start )") != 2:
        raise AssertionError("Both substring constructors must avoid signed end-start overflow")

    combined = header + "\n" + source
    for expression in (
        "EnsureAlloced( l + 1",
        "EnsureAlloced( len + 2",
        "EnsureAlloced( newLen + 1",
        "EnsureAlloced( len + l + 1",
        "EnsureAlloced( newlen + 1",
        "newLen = len + text.Length()",
        "newLen = len + l",
        "len + ( ( newLen - oldLen ) * count )",
    ):
        reject(combined, expression, "idStr allocation call sites")

    for helper in ("SaturatingAdd", "SaturatingMultiply", "TryRoundUpToInt"):
        require(allocation, helper, "allocation arithmetic helper")
        require(probe, helper, "native allocation boundary probe")

    for boundary in ("maximumRoundedAllocation + 1", "INT_MAX", "numeric_limits<size_t>::max()"):
        require(probe, boundary, "native allocation boundary probe")

    print("string_allocation_contract: ok")


if __name__ == "__main__":
    main()
