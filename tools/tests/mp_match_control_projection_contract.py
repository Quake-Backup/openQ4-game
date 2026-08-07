#!/usr/bin/env python3
"""Static and native contracts for Match Control presentation projection."""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "src/mpgame/mp/match/MatchControlProjection.h"
SOURCE = ROOT / "src/mpgame/mp/match/MatchControlProjection.cpp"

SCALAR_STATES = {
    "match_surface_available",
    "match_phase",
    "match_status_lines",
    "match_ready_action",
    "match_team_lock_action",
    "match_action_side_label",
    "match_action_side_0_label",
    "match_action_side_1_label",
    "match_action_side_visible",
    "match_action_side_0_enabled",
    "match_action_side_1_enabled",
    "match_action_side_0_selected",
    "match_action_side_1_selected",
    "match_broadcaster_control_visible",
    "match_broadcaster_action",
    "match_referee_authenticated",
    "match_global_proposal",
    "match_side_proposal",
    "match_proposal_scope_choice",
    "match_rules_summary",
    "match_staged_summary",
    "match_series_summary",
    "match_evidence_summary",
    "match_result_message",
    "match_role_choice",
    "match_rule_value",
    "match_series_profile_choice",
}

LIST_STATES = {
    "match_team_rows",
    "match_replacement_rows",
    "match_proposal_rows",
    "match_profile_rows",
    "match_rule_rows",
    "match_series_map_rows",
    "match_series_history_rows",
    "match_evidence_rows",
}

CONTEXT_STATES = {
    "match_context_visible",
    "match_context_phase",
    "match_context_role",
    "match_context_pause",
    "match_context_readiness",
    "match_context_timeouts",
    "match_context_proposal",
    "match_context_series",
    "match_context_items",
}

AVAILABILITY_PREFIXES = [
    "ready_set",
    "team_ready_set",
    "force_ready",
    "team_join",
    "team_lock_set",
    "queue_join",
    "queue_defer",
    "queue_leave",
    "roster_leave",
    "timeout_request",
    "tech_pause_request",
    "resume_request",
    "ref_authenticate",
    "ref_logout",
    "rules_select_profile",
    "rules_stage_field",
    "rules_commit",
    "rules_discard",
    "proposal_create",
    "proposal_cast",
    "proposal_cancel",
    "roster_invite",
    "roster_accept",
    "roster_remove",
    "roster_substitute",
    "role_assign",
    "broadcaster_set",
    "series_stage_profile",
    "series_start",
    "series_cancel",
    "series_advance",
    "veto_select",
    "forfeit",
    "abort",
    "participant_remove",
    "series_contestant_bind",
]


def read(path: Path) -> str:
    if not path.is_file():
        raise AssertionError(f"required source file not found: {path}")
    return path.read_text(encoding="utf-8", errors="strict")


def require(text: str, token: str, context: str) -> None:
    if token not in text:
        raise AssertionError(f"missing {token!r} in {context}")


def static_contracts(header: str, source: str) -> None:
    combined = header + source
    for state in sorted(SCALAR_STATES | LIST_STATES | CONTEXT_STATES):
        require(combined, f'"{state}"', "complete projection state set")

    registry = re.findall(
        r'\{\s*"([a-z0-9_]+)"\s*,\s*MP_MATCH_OP_[A-Z0-9_]+\s*\}', source
    )
    if registry != AVAILABILITY_PREFIXES:
        raise AssertionError(
            "operation availability registry drifted:\n"
            f"expected {AVAILABILITY_PREFIXES}\nactual   {registry}"
        )

    for token in (
        "MPMatchViewValidate( view, NULL )",
        "value.fieldId == MP_RULE_MANAGED_MATCH",
        "!ProjectionIsManagedMatch( acceptedView )",
        "model.SessionId() != state.sessionId",
        "model.ViewRevision() != state.viewRevision",
        "recipient.bindingGeneration != state.recipient.bindingGeneration",
        "context.localOperatorVisible ? 1 : 0",
        "context.resolveParticipantText",
        "context.resolveMapText",
        "MPMatchControlSanitizeDisplayText",
        "DeleteFirstUnusedListItem",
        "availability->reason == MP_MATCH_PROTOCOL_REASON_OK",
        "model->OperationContextAccepted( state.opcode )",
        "MPMatchControlLocalizationKey",
        "MPMatchControlProtocolReasonKey",
        "MPMatchControlReadinessBlockerKey",
        "MPMatchControlRuleFieldKey",
        "MPMatchControlErrorReasonKey",
        "MP_MATCH_OP_ROSTER_LEAVE",
        "model->CanChooseActionSide( 0 )",
        "model->CanChooseActionSide( 1 )",
        "model->ActionSideUsesCompetitionLabels()",
        "BuildRecipientText( value, sizeof( value ), acceptedView )",
        "BuildItemTimingText( value, sizeof( value ), acceptedView )",
        "ItemTimingOrdinal( token, \"large_armor\", ordinal )",
        "text.AppendUInt( static_cast<unsigned int>( ordinal ) )",
        "view.publicState.clocks.matchTimeMsec",
        "Adapter tokens are machine identifiers and are never rendered directly",
        "initializeChoices",
        "Cross-session results are rejected",
        "single visibility gate is deliberately written last",
    ):
        require(combined, token, "fail-closed projection boundary")

    if "text.Append( timing.token )" in source:
        raise AssertionError("raw item-timing machine token reaches presentation")

    # Presentation may write typed selection indices, but it must not read or
    # parse GUI/display state, refresh the GUI mid-batch, or expose credentials.
    for forbidden in (
        "StateChanged(",
        "GetStateString(",
        "GetStateInt(",
        "atoi(",
        "sscanf(",
        "strtok(",
        "match_referee_credential",
        ".key )",
        "row.key",
    ):
        if forbidden in source:
            raise AssertionError(
                f"projection contains forbidden presentation dependency {forbidden!r}"
            )

    # Map tokens may cross only the explicit resolver callback and must never
    # be assigned directly to GUI state.
    if re.search(r"SetStateString\([^;]*(?:mapToken|nextMap)", source, re.S):
        raise AssertionError("stable map token is exposed directly as display text")

    # Normal refreshes cannot overwrite caller-owned input/choice states.
    choice_writes = {
        state: source.count(f'SetStateString( "{state}"')
        for state in (
            "match_role_choice",
            "match_proposal_scope_choice",
            "match_series_profile_choice",
            "match_rule_value",
        )
    }
    if any(count != 1 for count in choice_writes.values()):
        raise AssertionError(f"choice states are written outside one-time initialization: {choice_writes}")

    listed = subprocess.run(
        [
            sys.executable,
            str(ROOT / "src/buildscripts/list_sources.py"),
            str(ROOT / "src"),
            "mpgame",
            "mpgame/Callbacks.cpp",
            "mpgame/gamesys/Callbacks.cpp",
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if listed.returncode != 0:
        raise AssertionError("could not inspect MP source discovery:\n" + listed.stderr)
    if "mpgame/mp/match/MatchControlProjection.cpp" not in listed.stdout.splitlines():
        raise AssertionError("MatchControlProjection.cpp is not compiled into the MP game module")


HARNESS = r'''
#include <string.h>

#define MP_MATCH_CONTROL_PROJECTION_SANITIZER_STANDALONE_TEST 1
#include "mpgame/mp/match/MatchControlProjection.cpp"

#define CHECK(condition) do { if (!(condition)) { return __LINE__; } } while (0)

int main(void) {
    char output[64];
    CHECK(MPMatchControlSanitizeDisplayText(NULL, output, sizeof(output)) == 0);
    CHECK(output[0] == '\0');
    CHECK(MPMatchControlSanitizeDisplayText("  Alice\t\r\n Bob  ", output,
        sizeof(output)) == 9);
    CHECK(strcmp(output, "Alice Bob") == 0);

    const char validUtf8[] = { 'R', (char)0xc3, (char)0xa9, 'n', (char)0xc3,
        (char)0xa9, 0 };
    CHECK(MPMatchControlSanitizeDisplayText(validUtf8, output, sizeof(output)) == 6);
    CHECK(memcmp(output, validUtf8, sizeof(validUtf8)) == 0);

    const char invalidUtf8[] = { 'A', (char)0xc0, (char)0xaf, 'B', 0 };
    CHECK(MPMatchControlSanitizeDisplayText(invalidUtf8, output, sizeof(output)) == 4);
    CHECK(strcmp(output, "A??B") == 0);

    const char euro[] = { 'a', 'b', (char)0xe2, (char)0x82, (char)0xac, 0 };
    char shortOutput[4];
    CHECK(MPMatchControlSanitizeDisplayText(euro, shortOutput,
        sizeof(shortOutput)) == 2);
    CHECK(strcmp(shortOutput, "ab") == 0);

    const char controls[] = { 'a', 1, 2, '\t', 'b', 0 };
    CHECK(MPMatchControlSanitizeDisplayText(controls, output, sizeof(output)) == 3);
    CHECK(strcmp(output, "a b") == 0);
    CHECK(MPMatchControlSanitizeDisplayText(
        "^1Red^0 ^c683Marine^i123Icon^rName", output,
        sizeof(output)) == 18);
    CHECK(strcmp(output, "Red MarineIconName") == 0);
    CHECK(MPMatchControlSanitizeDisplayText("^^literal ^xkept", output,
        sizeof(output)) == 16);
    CHECK(strcmp(output, "^^literal ^xkept") == 0);
    CHECK(MPMatchControlSanitizeDisplayText("abc", NULL, 0) == 0);
    return 0;
}
'''


def native_contracts() -> None:
    compiler = next(
        (path for name in ("clang++", "g++", "c++") if (path := shutil.which(name))),
        None,
    )
    if compiler is None:
        print("mp_match_control_projection_contract: native checks skipped (no C++ compiler)")
        return

    temp_root = ROOT / ".tmp"
    temp_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="match-control-projection-", dir=temp_root) as temp:
        temp_dir = Path(temp)
        harness = temp_dir / "match_control_projection_contract.cpp"
        executable = temp_dir / (
            "match_control_projection_contract.exe"
            if compiler.lower().endswith(".exe")
            else "match_control_projection_contract"
        )
        harness.write_text(HARNESS, encoding="utf-8")
        compiled = subprocess.run(
            [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                f"-I{ROOT / 'src'}",
                str(harness),
                "-o",
                str(executable),
            ],
            cwd=ROOT,
            text=True,
            capture_output=True,
        )
        if compiled.returncode != 0:
            raise AssertionError(
                "standalone projection sanitizer did not compile:\n"
                + compiled.stdout
                + compiled.stderr
            )
        ran = subprocess.run([str(executable)], cwd=ROOT, text=True, capture_output=True)
        if ran.returncode != 0:
            raise AssertionError(
                "projection sanitizer invariant failed at harness line "
                f"{ran.returncode}:\n{ran.stdout}{ran.stderr}"
            )


def main() -> None:
    header = read(HEADER)
    source = read(SOURCE)
    static_contracts(header, source)
    native_contracts()
    print("mp_match_control_projection_contract: PASS")


if __name__ == "__main__":
    main()
