#!/usr/bin/env python3
"""Guard the non-consuming lexer lookahead required by bot characters."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"Missing {signature!r}")

    opening = source.find("{", start + len(signature))
    if opening < 0:
        raise AssertionError(f"Missing body for {signature!r}")

    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1 : index]

    raise AssertionError(f"Unterminated body for {signature!r}")


def main() -> None:
    header = read("src/idlib/Lexer.h")
    source = (ROOT / "src/idlib/Lexer.cpp").read_text(encoding="windows-1252")
    bot_character = read("src/mpgame/bots/BotCharacter.cpp")

    declarations = re.findall(
        r"\bint\s+PeekTokenString\s*\(\s*const char \*string\s*\)\s*;",
        header,
    )
    if len(declarations) != 2:
        raise AssertionError(
            "idLexer and Lexer must both expose PeekTokenString; "
            f"found {len(declarations)} declarations"
        )

    native = function_body(source, "int idLexer::PeekTokenString")
    for token in ("ReadToken", "UnreadToken( &tok )", "tok == string"):
        if token not in native:
            raise AssertionError(f"idLexer lookahead lost non-consuming behavior: {token!r}")
    for cursor_rewind in ("script_p = lastScript_p", "line = lastline"):
        if cursor_rewind in native:
            raise AssertionError(
                f"idLexer lookahead must restore complete token state via UnreadToken: {cursor_rewind!r}"
            )
    if native.index("ReadToken") > native.index("UnreadToken( &tok )"):
        raise AssertionError("idLexer lookahead must read before restoring the token")

    wrapper = function_body(source, "int Lexer::PeekTokenString")
    for token in ("mDelegate->PeekTokenString", "ReadToken", "UnreadToken", "tok == string"):
        if token not in wrapper:
            raise AssertionError(f"Lexer lookahead lost delegation/unread behavior: {token!r}")

    if 'lexer.PeekTokenString( "{" )' not in bot_character:
        raise AssertionError("BotCharacter parser no longer exercises lexer lookahead")

    print("lexer_peek_contract: ok")


if __name__ == "__main__":
    main()
