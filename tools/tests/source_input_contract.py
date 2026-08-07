#!/usr/bin/env python3
"""Static checks for the openQ4-GameLibs source-input contract."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def reject(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        raise AssertionError(f"Unexpected {needle!r} in {context}")


def main() -> None:
    readme = read("README.md")
    agents = read("AGENTS.md")
    src_meson = read("src/meson.build")
    lib_h = read("src/idlib/Lib.h")
    workflow = read(".github/workflows/commit-validation.yml")

    require(readme, "canonical source-input repository", "README source-input overview")
    require(readme, "openQ4 consumes this repository as source input", "README integration contract")
    require(readme, "Standalone Linux builds are supported", "README standalone Linux build support")
    require(readme, "openQ4's staged engine build provides integrated runtime modules", "README Linux integration path")

    require(agents, "canonical source-input repository", "AGENTS cross-repo workflow")
    require(agents, "standalone Linux x64 and arm64 SP/MP module builds supported", "AGENTS Linux build support")
    require(agents, "Do not maintain or edit an openQ4 `src/game` mirror", "AGENTS mirror warning")

    require(src_meson, "is_linux = host_system == 'linux'", "Meson Linux host predicate")
    require(
        src_meson,
        "openQ4-GameLibs Linux builds require GCC or Clang.",
        "Meson Linux compiler contract",
    )

    require(lib_h, "if ( text != NULL )", "null-safe idException construction")
    require(
        lib_h,
        "i < (int)sizeof( error ) - 1 && text[i] != '\\0'",
        "bounded idException construction",
    )
    require(lib_h, "error[i] = '\\0';", "terminated idException construction")
    reject(lib_h, "strcpy( error, text )", "idException construction")

    require(workflow, "runs-on: ubuntu-24.04", "Linux CI runner")
    require(workflow, "Linux x64 GameLibs", "Linux standalone CI job")
    require(workflow, "Linux ARM64 GameLibs", "Linux ARM64 standalone CI job")
    require(workflow, "meson compile -C builddir", "Linux standalone CI compile")
    require(workflow, "tools/tests/lexer_peek_contract.py", "workflow lexer lookahead test compile")
    require(workflow, "python tools/tests/lexer_peek_contract.py", "workflow lexer lookahead test run")
    require(workflow, "tools/tests/source_input_contract.py", "workflow source-input test compile")
    require(workflow, "python tools/tests/source_input_contract.py", "workflow source-input test run")

    print("source_input_contract: ok")


if __name__ == "__main__":
    main()
