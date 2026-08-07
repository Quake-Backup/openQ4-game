#!/usr/bin/env python3
"""Hostile executable and adapter-order contracts for population termination."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "src/mpgame/mp/match/MatchTerminationPolicy.h"
SOURCE = ROOT / "src/mpgame/mp/match/MatchTerminationPolicy.cpp"
MULTIPLAYER = ROOT / "src/mpgame/MultiplayerGame.cpp"
GAME_STATE = ROOT / "src/mpgame/mp/GameState.cpp"


def require(text: str, token: str, context: str) -> None:
    if token not in text:
        raise AssertionError(f"missing {token!r} in {context}")


def bounded(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


def static_contracts() -> None:
    header = HEADER.read_text(encoding="utf-8", errors="strict")
    source = SOURCE.read_text(encoding="utf-8", errors="strict")
    multiplayer = MULTIPLAYER.read_text(encoding="utf-8", errors="strict")
    game_state = GAME_STATE.read_text(encoding="utf-8", errors="strict")

    for forbidden in (
        "gameLocal",
        "idPlayer",
        "idCVar",
        "idFile",
        "cmdSystem",
        "networkSystem",
        "teamScore",
    ):
        if forbidden in header + source:
            raise AssertionError(
                f"termination policy contains adapter dependency {forbidden!r}"
            )

    for token in (
        "MPEvaluatePopulationTermination",
        "MP_MATCH_TERMINATION_COUNTDOWN_CANCELLED",
        "MP_MATCH_TRANSITION_COUNTDOWN_ABORTED",
        "MP_MATCH_TRANSITION_MATCH_ABORTED",
        "MP_MATCH_TRANSITION_FORFEIT",
    ):
        require(header + source, token, "pure termination policy")

    abort_adapter = bounded(
        multiplayer,
        "void idMultiplayerGame::CheckAbortGame( mpParticipantId",
        "idMultiplayerGame::WantKilled",
    )
    for token in (
        "MPEvaluatePopulationTermination( phase, enoughClients, forfeitingSide )",
        "CommitMatchPhaseTransition( decision.targetPhase, decision.reason",
        "decision.forfeitingSide",
        "matchSeriesContestantConnection[ opponentSide ] ==",
        "MP_SERIES_MAP_ACTIVE",
    ):
        require(abort_adapter, token, "population-loss adapter")
    for forbidden in (
        "TimeLimitHit()",
        "GetMatchLengthMsec()",
        "gameState->NewState( WARMUP )",
        "gameState->NewState( GAMEREVIEW )",
    ):
        if forbidden in abort_adapter:
            raise AssertionError(
                f"population-loss adapter retains inferred legacy branch {forbidden!r}"
            )

    forfeit_team = bounded(
        multiplayer,
        "int idMultiplayerGame::ForfeitTeam",
        "idMultiplayerGame::GetOvertimeRespawnDelay",
    )
    require(
        forfeit_team,
        "matchRules.Committed().GetBool( MP_RULE_FORFEIT_ON_EMPTY_TEAM )",
        "managed forfeit rule authority",
    )

    effects = bounded(
        multiplayer,
        "bool idMultiplayerGame::ApplyCommittedMatchPhaseEffects",
        "bool idMultiplayerGame::CommitMatchPhaseTransition( mpGameState_t newState,\n\t\tmpMatchTransitionReason_t reason, mpParticipantId authorizer,",
    )
    for token in (
        "matchPhaseEffectsSessionId == matchSession.GetSessionId()",
        "matchPhaseEffectsRevision == matchSession.GetSessionRevision()",
        "RecordMatchEvidenceResult( transition.reason, transition.authorizer,",
    ):
        require(effects, token, "exact-once committed transition effects")
    for forbidden in (
        "CommitCompetitionSeriesResult(",
        "CommitCompetitionSeriesMapEvidence(",
        ".CommitMapResult(",
        "matchSeries =",
        "matchSeriesReport =",
    ):
        if forbidden in effects:
            raise AssertionError(
                "committed transition effects publish series state before "
                f"evidence sealing via {forbidden!r}"
            )

    finalizer = bounded(
        multiplayer,
        "bool idMultiplayerGame::FinalizeMatchEvidence",
        "idMultiplayerGame::BeginMatchSession",
    )
    require(
        finalizer,
        "CommitCompetitionSeriesMapEvidence( evidenceStorage )",
        "evidence-sealed series result transaction",
    )
    if finalizer.index("CommitCompetitionSeriesMapEvidence( evidenceStorage )") > \
            finalizer.index("matchEvidenceFinalized = true"):
        raise AssertionError(
            "evidence is marked finalized before the series/report transaction commits"
        )

    series_seal = bounded(
        multiplayer,
        "bool idMultiplayerGame::CommitCompetitionSeriesMapEvidence",
        "void idMultiplayerGame::StartMatchMVDIfRequired",
    )
    for token in (
        "seriesCandidate.CommitMapResult(",
        "reportCandidate.AppendMapResult(",
        "PersistCompetitionSeriesCandidate( seriesCandidate, reportCandidate,",
        "matchSeries = seriesCandidate;",
        "matchSeriesReport = reportCandidate;",
    ):
        require(series_seal, token, "sealed series/report publication")
    if not (
        series_seal.index("seriesCandidate.CommitMapResult(")
        < series_seal.index("reportCandidate.AppendMapResult(")
        < series_seal.index(
            "PersistCompetitionSeriesCandidate( seriesCandidate, reportCandidate,"
        )
        < series_seal.index("matchSeries = seriesCandidate;")
        < series_seal.index("matchSeriesReport = reportCandidate;")
    ):
        raise AssertionError(
            "sealed series/report candidates are not checkpointed before publication"
        )

    mirror = bounded(
        multiplayer,
        "void idMultiplayerGame::ApplyMatchOperationLegacyMirror",
        "void idMultiplayerGame::ServerReceiveMatchOperation",
    )
    for token in (
        "MPOperationMapProtocolTeam( request.teamTarget, forfeitingSide )",
        "CommitMatchPhaseTransition( transition.to, transition.reason,",
        "transition.authorizer, forfeitingSide",
    ):
        require(mirror, token, "typed loser/reason propagation")

    tourney = bounded(
        game_state,
        "bool rvTourneyGameState::NewState",
        "rvTourneyGameState::GameStateChanged",
    )
    require(
        tourney,
        "GetMatchSession().GetPhase() != newState",
        "already-committed Tourney transition mirror",
    )


PROBE = r'''
#include "src/mpgame/mp/match/MatchTerminationPolicy.h"

#include <stdio.h>

static int failures = 0;

static void CheckNone(mpGameState_t phase, bool enough, int side) {
    const mpMatchTerminationDecision decision =
        MPEvaluatePopulationTermination(phase, enough, side);
    if (decision.ShouldTransition() || decision.kind != MP_MATCH_TERMINATION_NONE ||
            decision.reason != MP_MATCH_TRANSITION_NONE) {
        ++failures;
    }
}

static void Check(mpGameState_t phase, int side,
        mpMatchTerminationKind_t kind, mpGameState_t target,
        mpMatchTransitionReason_t reason, int expectedSide) {
    const mpMatchTerminationDecision decision =
        MPEvaluatePopulationTermination(phase, false, side);
    if (!decision.ShouldTransition() || decision.kind != kind ||
            decision.targetPhase != target || decision.reason != reason ||
            decision.forfeitingSide != expectedSide) {
        ++failures;
    }
}

int main(void) {
    CheckNone(INACTIVE, false, 0);
    CheckNone(WARMUP, false, 0);
    CheckNone(GAMEREVIEW, false, 0);
    CheckNone(NEXTGAME, false, 0);
    CheckNone(COUNTDOWN, true, 0);
    CheckNone(GAMEON, true, 0);
    CheckNone(SUDDENDEATH, true, 1);

    Check(COUNTDOWN, 0, MP_MATCH_TERMINATION_COUNTDOWN_CANCELLED,
        WARMUP, MP_MATCH_TRANSITION_COUNTDOWN_ABORTED, MP_MATCH_SIDE_NONE);
    Check(COUNTDOWN, 1, MP_MATCH_TERMINATION_COUNTDOWN_CANCELLED,
        WARMUP, MP_MATCH_TRANSITION_COUNTDOWN_ABORTED, MP_MATCH_SIDE_NONE);
    Check(GAMEON, MP_MATCH_SIDE_NONE, MP_MATCH_TERMINATION_ABORTED,
        GAMEREVIEW, MP_MATCH_TRANSITION_MATCH_ABORTED, MP_MATCH_SIDE_NONE);
    Check(SUDDENDEATH, -2, MP_MATCH_TERMINATION_ABORTED,
        GAMEREVIEW, MP_MATCH_TRANSITION_MATCH_ABORTED, MP_MATCH_SIDE_NONE);
    Check(GAMEON, MP_MATCH_SIDE_COUNT, MP_MATCH_TERMINATION_ABORTED,
        GAMEREVIEW, MP_MATCH_TRANSITION_MATCH_ABORTED, MP_MATCH_SIDE_NONE);
    Check(GAMEON, 0, MP_MATCH_TERMINATION_FORFEIT,
        GAMEREVIEW, MP_MATCH_TRANSITION_FORFEIT, 0);
    Check(SUDDENDEATH, 1, MP_MATCH_TERMINATION_FORFEIT,
        GAMEREVIEW, MP_MATCH_TRANSITION_FORFEIT, 1);

    if (failures != 0) {
        fprintf(stderr, "termination policy failures: %d\n", failures);
        return 1;
    }
    return 0;
}
'''


def executable_contract() -> None:
    compiler = next(
        (candidate for candidate in ("clang++", "g++", "cl") if shutil.which(candidate)),
        None,
    )
    if compiler is None:
        print(
            "mp_match_termination_policy_contract: executable checks skipped "
            "(no C++ compiler)"
        )
        return

    scratch_root = ROOT / ".tmp"
    scratch_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="mp-match-termination-", dir=scratch_root
    ) as directory:
        temporary = Path(directory)
        probe = temporary / "probe.cpp"
        probe.write_text(PROBE, encoding="utf-8")
        executable = temporary / (
            "probe.exe" if Path(compiler).name.lower() == "cl.exe" else "probe"
        )
        if Path(compiler).name.lower() in ("cl", "cl.exe"):
            command = [
                compiler,
                "/nologo",
                "/std:c++17",
                "/EHsc",
                "/DMP_MATCH_TERMINATION_STANDALONE",
                f"/I{ROOT}",
                str(SOURCE),
                str(probe),
                f"/Fe:{executable}",
            ]
        else:
            command = [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-DMP_MATCH_TERMINATION_STANDALONE",
                f"-I{ROOT}",
                str(SOURCE),
                str(probe),
                "-o",
                str(executable),
            ]
        compiled = subprocess.run(
            command, cwd=ROOT, text=True, capture_output=True, check=False
        )
        if compiled.returncode != 0:
            raise AssertionError(
                "termination policy probe did not compile:\n"
                + compiled.stdout
                + compiled.stderr
            )
        ran = subprocess.run(
            [str(executable)], cwd=ROOT, text=True, capture_output=True, check=False
        )
        if ran.returncode != 0:
            raise AssertionError(
                "termination policy probe failed:\n" + ran.stdout + ran.stderr
            )


def main() -> None:
    static_contracts()
    executable_contract()
    print("mp_match_termination_policy_contract: PASS")


if __name__ == "__main__":
    main()
