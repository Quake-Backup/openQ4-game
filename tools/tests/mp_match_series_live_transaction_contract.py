#!/usr/bin/env python3
"""Live adapter contracts for atomic series, report, and evidence publication."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MULTIPLAYER_HEADER = ROOT / "src/mpgame/MultiplayerGame.h"
MULTIPLAYER_SOURCE = ROOT / "src/mpgame/MultiplayerGame.cpp"
OPERATIONS_SOURCE = ROOT / "src/mpgame/mp/match/MatchOperations.cpp"
RECOVERY_HEADER = ROOT / "src/mpgame/mp/match/MatchSeriesRecovery.h"
RECOVERY_SOURCE = ROOT / "src/mpgame/mp/match/MatchSeriesRecovery.cpp"


def require(text: str, token: str, context: str) -> None:
    if token not in text:
        raise AssertionError(f"missing {token!r} in {context}")


def require_order(text: str, tokens: tuple[str, ...], context: str) -> None:
    cursor = -1
    for token in tokens:
        position = text.find(token, cursor + 1)
        if position < 0:
            raise AssertionError(f"missing {token!r} in {context}")
        if position <= cursor:
            raise AssertionError(f"out-of-order {token!r} in {context}")
        cursor = position


def region(text: str, start: str, end: str, context: str) -> str:
    begin = text.find(start)
    if begin < 0:
        raise AssertionError(f"missing start marker {start!r} in {context}")
    finish = text.find(end, begin + len(start))
    if finish < 0:
        raise AssertionError(f"missing end marker {end!r} in {context}")
    return text[begin:finish]


def function(text: str, signature: str, context: str) -> str:
    """Extract one C++ definition without depending on the next function name."""

    begin = text.find(signature)
    if begin < 0:
        raise AssertionError(f"missing function {signature!r} in {context}")
    opening = text.find("{", begin + len(signature))
    if opening < 0:
        raise AssertionError(f"missing function body for {signature!r} in {context}")
    depth = 0
    for index in range(opening, len(text)):
        character = text[index]
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return text[begin : index + 1]
    raise AssertionError(f"unterminated function {signature!r} in {context}")


def member_contracts(header: str, source: str) -> None:
    for token in (
        '#include "mp/match/MatchSeriesReport.h"',
        '#include "mp/match/MatchSeriesReportStorage.h"',
        "mpCompetitionSeriesReport matchSeriesReport;",
        "mpSeriesReportStorageWorkspace matchSeriesReportWorkspace;",
        "bool\t\t\tmatchSeriesAwaitingMapSession;",
        "PersistCompetitionSeriesCandidate(",
        "CommitCompetitionSeriesMapEvidence(",
    ):
        require(header, token, "multiplayer series/report ownership")
    require(
        source,
        '#include "mp/match/MatchSeriesReportFileSystem.h"',
        "live report filesystem adapter",
    )


def recovery_contracts(
    multiplayer: str, recovery_header: str, recovery_source: str
) -> None:
    require(
        recovery_header,
        "MP_SERIES_RECOVERY_SCHEMA_VERSION = 4",
        "unified checkpoint schema",
    )
    require(
        recovery_header,
        "MP_SERIES_RECOVERY_PREVIOUS_SCHEMA_VERSION = 3",
        "schema-3 paired checkpoint compatibility",
    )
    paired_core = function(
        recovery_source,
        "bool MPMatchSeriesRecoveryRestoreCores",
        "paired recovery core",
    )
    require_order(
        paired_core,
        (
            "if ( !record.hasReport )",
            "candidateSeries.RestoreRecoveryState( record.series )",
            "candidateReport.RestoreCheckpointState( record.report )",
            "ValidateReportAgainstSeries( candidateSeries, record.seriesId,",
            "series = candidateSeries;",
            "report = candidateReport;",
        ),
        "paired recovery core",
    )

    restore = function(
        multiplayer,
        "bool idMultiplayerGame::RestoreCompetitionSeriesIfRequested",
        "live paired recovery",
    )
    require_order(
        restore,
        (
            "MPMatchSeriesRecoveryLoadFileSystem(",
            "if ( !record.hasReport )",
            "MPMatchSeriesRecoveryRestoreCores( record, candidate, reportCandidate,",
            "reportCandidate.GetIdentity().rulesDigest",
            "matchSeries = candidate;",
            "matchSeriesReport = reportCandidate;",
            "matchSeriesId = record.seriesId;",
            "matchSeriesLinkedSessionId = record.linkedSessionId;",
        ),
        "live paired recovery",
    )
    if "MPMatchSeriesRecoveryRestore(" in restore:
        raise AssertionError("live recovery retains the unpaired legacy restore path")


def initial_publish_contract(multiplayer: str) -> None:
    configure = region(
        multiplayer,
        "if ( kind == MP_OPERATION_CONTINUATION_SERIES_CONFIGURE_PROFILE )",
        "if ( kind == MP_OPERATION_CONTINUATION_SERIES_ADVANCE_AND_LOAD_MAP )",
        "series configuration continuation",
    )
    require_order(
        configure,
        (
            "mpCompetitionSeriesReport reportCandidate;",
            "InitializeCompetitionSeriesReport( candidate, newSeriesId,",
            "PersistCompetitionSeriesCandidate( candidate, reportCandidate,",
            "matchSeries = candidate;",
            "matchSeriesReport = reportCandidate;",
            "matchSeriesId = newSeriesId;",
        ),
        "series configuration transaction",
    )


def map_session_handoff_contracts(multiplayer: str) -> None:
    constructor = function(
        multiplayer,
        "idMultiplayerGame::idMultiplayerGame()",
        "map-session handoff initialization",
    )
    require(
        constructor,
        "matchSeriesAwaitingMapSession = false;",
        "map-session handoff initialization",
    )

    restore = function(
        multiplayer,
        "bool idMultiplayerGame::RestoreCompetitionSeriesIfRequested",
        "restored map-session handoff",
    )
    require_order(
        restore,
        (
            "matchSeries = candidate;",
            "matchSeriesReport = reportCandidate;",
            "matchSeriesAwaitingMapSession =",
            "matchSeries.GetState() == MP_SERIES_MAP_ACTIVE;",
        ),
        "restored MAP_ACTIVE handoff",
    )

    schedule = function(
        multiplayer,
        "bool idMultiplayerGame::ScheduleCompetitionSeriesMap",
        "scheduled map-session handoff",
    )
    require_order(
        schedule,
        (
            "candidate.BeginMap( mapToken,",
            "PersistCompetitionSeriesCandidate( candidate, matchSeriesReport,",
            "matchSeries = candidate;",
            "matchSeriesAwaitingMapSession = true;",
            'gameLocal.sessionCommand = "nextMap";',
        ),
        "checkpointed BeginMap handoff",
    )

    finalizer = function(
        multiplayer,
        "bool idMultiplayerGame::FinalizeMatchEvidence",
        "pre-bind evidence finalizer",
    )
    require_order(
        finalizer,
        (
            "matchSeries.GetState() == MP_SERIES_MAP_ACTIVE",
            "!matchSeriesAwaitingMapSession",
            "!CommitCompetitionSeriesMapEvidence( evidenceStorage )",
        ),
        "pre-bind series commit suppression",
    )

    begin_session = function(
        multiplayer,
        "bool idMultiplayerGame::BeginMatchSession",
        "successful runtime session binding",
    )
    require_order(
        begin_session,
        (
            "matchSeries.GetState() == MP_SERIES_MAP_ACTIVE",
            "PersistCompetitionSeriesCandidate( matchSeries, matchSeriesReport,",
            "matchSeriesLinkedSessionId = matchSession.GetSessionId();",
            "matchSeriesAwaitingMapSession = false;",
        ),
        "successful runtime session binding",
    )

    rollback_functions = (
        "void idMultiplayerGame::ProcessPassedMatchProposals",
        "void idMultiplayerGame::ServerReceiveMatchOperation",
        "bool idMultiplayerGame::ExecuteTrustedLocalMatchOperation",
    )
    for signature in rollback_functions:
        rollback = function(multiplayer, signature, "operation rollback snapshot")
        require_order(
            rollback,
            (
                "const bool awaitingMapSessionBeforeExecution =",
                "matchSeriesAwaitingMapSession;",
                "execution.outcome == MP_OPERATION_REJECTED",
                "matchSeries = seriesBeforeExecution;",
                "matchSeriesReport = reportBeforeExecution;",
                "matchSeriesLinkedSessionId = linkedSessionBeforeExecution;",
                "matchSeriesAwaitingMapSession = awaitingMapSessionBeforeExecution;",
            ),
            f"{signature} rollback snapshot",
        )


def evidence_commit_contracts(multiplayer: str) -> None:
    effects = function(
        multiplayer,
        "bool idMultiplayerGame::ApplyCommittedMatchPhaseEffects",
        "committed phase effects",
    )
    require(
        effects,
        "RecordMatchEvidenceResult( transition.reason, transition.authorizer,",
        "committed phase result journal",
    )
    for forbidden in (
        "CommitCompetitionSeriesMapEvidence(",
        ".CommitMapResult(",
        "matchSeries =",
        "matchSeriesReport =",
    ):
        if forbidden in effects:
            raise AssertionError(
                "phase effects bypass evidence sealing with " f"{forbidden!r}"
            )

    finalizer = function(
        multiplayer,
        "bool idMultiplayerGame::FinalizeMatchEvidence",
        "evidence finalizer",
    )
    require_order(
        finalizer,
        (
            "RecordMatchEvidenceFinalStats();",
            "StopMatchMVD(",
            "PersistMatchEvidence( &evidenceStorage )",
            "CommitCompetitionSeriesMapEvidence( evidenceStorage )",
            "return false;",
            "matchEvidenceFinalized = true;",
        ),
        "evidence sealing boundary",
    )
    failed_commit = finalizer.index(
        "CommitCompetitionSeriesMapEvidence( evidenceStorage )"
    )
    finalized = finalizer.index("matchEvidenceFinalized = true;")
    if "matchEvidenceFinalized = true;" in finalizer[:failed_commit] or not (
        failed_commit < finalized
    ):
        raise AssertionError(
            "a failed paired checkpoint can still mark match evidence finalized"
        )

    commit = function(
        multiplayer,
        "bool idMultiplayerGame::CommitCompetitionSeriesMapEvidence",
        "sealed map publication",
    )
    require_order(
        commit,
        (
            "mpCompetitionSeries seriesCandidate = matchSeries;",
            "seriesCandidate.CommitMapResult(",
            "mpCompetitionSeriesReport reportCandidate = matchSeriesReport;",
            "reportCandidate.AppendMapResult(",
            "PersistCompetitionSeriesCandidate( seriesCandidate, reportCandidate,",
            "matchSeries = seriesCandidate;",
            "matchSeriesReport = reportCandidate;",
            "matchSeriesLinkedSessionId = matchSession.GetSessionId();",
            "matchSeriesAwaitingMapSession = false;",
        ),
        "sealed map candidate checkpoint",
    )
    checkpoint = commit.index(
        "PersistCompetitionSeriesCandidate( seriesCandidate, reportCandidate,"
    )
    if "matchSeries =" in commit[:checkpoint] or "matchSeriesReport =" in commit[:checkpoint]:
        raise AssertionError("series/report state is published before its checkpoint")


def terminal_report_contract(multiplayer: str) -> None:
    terminal = function(
        multiplayer,
        "bool idMultiplayerGame::FinalizeCompetitionSeriesReport",
        "terminal report transaction",
    )
    require_order(
        terminal,
        (
            "report.Finalize( finalInput )",
            "MPMatchSeriesReportStoragePersist(",
            "PersistCompetitionSeriesCandidate( series, report, matchSeriesId,",
        ),
        "terminal report JSON and checkpoint ordering",
    )
    require(
        terminal,
        "MP_EVIDENCE_OUTPUT_SERIES_REPORT",
        "typed terminal-report failure evidence",
    )
    require(
        terminal,
        "MP_EVIDENCE_OUTPUT_SERIES_RECOVERY",
        "typed terminal-checkpoint failure evidence",
    )


def mutation_guard_contracts(operations: str) -> None:
    series_owner = function(
        operations,
        "static bool SeriesOwnsCommittedRules",
        "series-owned rules guard",
    )
    for terminal in (
        "state != MP_SERIES_DISABLED",
        "state != MP_SERIES_COMPLETE",
        "state != MP_SERIES_CANCELLED",
    ):
        require(series_owner, terminal, "series-owned rules guard")

    cancellation = region(
        operations,
        "case MP_MATCH_OP_SERIES_CANCEL:",
        "case MP_MATCH_OP_SERIES_ADVANCE:",
        "active-map cancellation guard",
    )
    require_order(
        cancellation,
        (
            "series.GetState() == MP_SERIES_MAP_ACTIVE",
            "Reject( MP_OPERATION_REASON_SERIES_STATE,",
            "series.Cancel(",
        ),
        "active-map cancellation guard",
    )

    select_profile = region(
        operations,
        "case MP_MATCH_OP_RULES_SELECT_PROFILE:",
        "case MP_MATCH_OP_RULES_STAGE_FIELD:",
        "series rules profile guard",
    )
    stage_field = region(
        operations,
        "case MP_MATCH_OP_RULES_STAGE_FIELD:",
        "case MP_MATCH_OP_RULES_COMMIT:",
        "series rules field guard",
    )
    commit_rules = region(
        operations,
        "case MP_MATCH_OP_RULES_COMMIT:",
        "case MP_MATCH_OP_RULES_DISCARD:",
        "series rules commit guard",
    )
    for scoped, context in (
        (select_profile, "series rules profile guard"),
        (stage_field, "series rules field guard"),
        (commit_rules, "series rules commit guard"),
    ):
        require(scoped, "SeriesOwnsCommittedRules( series )", context)
        require(scoped, "MP_OPERATION_REASON_RULE_STATE", context)


def artifact_status_contract(multiplayer: str) -> None:
    commit = function(
        multiplayer,
        "bool idMultiplayerGame::CommitCompetitionSeriesMapEvidence",
        "typed map artifact projection",
    )
    evidence = region(
        commit,
        "mpSeriesReportArtifactInput &evidenceArtifact",
        "mpSeriesReportArtifactInput &mvdArtifact",
        "evidence artifact status",
    )
    mvd = function(
        multiplayer,
        "void idMultiplayerGame::ProjectMatchMVDReportArtifact",
        "durable MVD artifact projection",
    )
    for scoped, kind, context in (
        (evidence, "MP_SERIES_REPORT_ARTIFACT_EVIDENCE", "evidence artifact status"),
        (mvd, "MP_SERIES_REPORT_ARTIFACT_MVD", "MVD artifact status"),
    ):
        for token in (
            kind,
            "MP_SERIES_REPORT_ARTIFACT_NOT_REQUESTED",
            "MP_SERIES_REPORT_ARTIFACT_AVAILABLE",
            "MP_SERIES_REPORT_ARTIFACT_FAILED",
            "MPMatchSeriesReportIsSafeArtifactQPath(",
        ):
            require(scoped, token, context)
    for token in (
        "MP_SERIES_REPORT_ARTIFACT_PENDING",
        "ServerCopyMVDRecordingResult",
        "MatchMVDResultForFinalQPath",
        "matchMVDOperatorOwnedBySession",
    ):
        require(mvd, token, "durable MVD artifact status")
    if "!networkSystem->ServerIsMVDRecording()" in mvd:
        raise AssertionError("MVD availability must not be inferred from idle state")


def main() -> None:
    header = MULTIPLAYER_HEADER.read_text(encoding="utf-8", errors="strict")
    multiplayer = MULTIPLAYER_SOURCE.read_text(encoding="utf-8", errors="strict")
    operations = OPERATIONS_SOURCE.read_text(encoding="utf-8", errors="strict")
    recovery_header = RECOVERY_HEADER.read_text(encoding="utf-8", errors="strict")
    recovery_source = RECOVERY_SOURCE.read_text(encoding="utf-8", errors="strict")

    member_contracts(header, multiplayer)
    recovery_contracts(multiplayer, recovery_header, recovery_source)
    initial_publish_contract(multiplayer)
    map_session_handoff_contracts(multiplayer)
    evidence_commit_contracts(multiplayer)
    terminal_report_contract(multiplayer)
    mutation_guard_contracts(operations)
    artifact_status_contract(multiplayer)
    print("mp_match_series_live_transaction_contract: PASS")


if __name__ == "__main__":
    main()
