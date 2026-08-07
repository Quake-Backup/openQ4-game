#!/usr/bin/env python3
"""Run every competitive-match contract from one cross-platform CI entry point."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TEST_ROOT = ROOT / "tools" / "tests"
EXPLICIT_CONTRACTS = (
    "mp_gametype_selectability.py",
    "mp_vote_security_contract.py",
)
REQUIRED_CONTRACTS = {
    "mp_match_authentication_contract.py",
    "mp_match_control_live_adapter_contract.py",
    "mp_match_control_actions_contract.py",
    "mp_match_control_model_contract.py",
    "mp_match_control_projection_contract.py",
    "mp_match_disclosure_policy_contract.py",
    "mp_match_evidence_contract.py",
    "mp_match_evidence_observer_contract.py",
    "mp_match_evidence_storage_contract.py",
    "mp_match_evidence_view_contract.py",
    "mp_match_item_timing_contract.py",
    "mp_match_legacy_admin_contract.py",
    "mp_match_live_adapter_contract.py",
    "mp_match_operations_contract.py",
    "mp_match_pause_contract.py",
    "mp_match_proposal_contract.py",
    "mp_match_protocol_contract.py",
    "mp_match_round_adapter_contract.py",
    "mp_match_rules_contract.py",
    "mp_match_series_binding_contract.py",
    "mp_match_series_checkpoint_contract.py",
    "mp_match_series_contract.py",
    "mp_match_series_live_transaction_contract.py",
    "mp_match_series_recovery_contract.py",
    "mp_match_series_recovery_filesystem_contract.py",
    "mp_match_series_report_contract.py",
    "mp_match_series_report_filesystem_contract.py",
    "mp_match_series_report_storage_contract.py",
    "mp_match_session_contract.py",
    "mp_match_team_communication_contract.py",
    "mp_match_teams_contract.py",
    "mp_match_termination_policy_contract.py",
    "mp_match_transaction_hardening_contract.py",
    "mp_match_view_contract.py",
}


def discover_contracts() -> list[Path]:
    discovered = {
        path.name: path for path in TEST_ROOT.glob("mp_match_*_contract.py")
    }
    missing = sorted(REQUIRED_CONTRACTS.difference(discovered))
    if missing:
        raise RuntimeError(
            "competitive match contract inventory is incomplete: "
            + ", ".join(missing)
        )

    for name in EXPLICIT_CONTRACTS:
        path = TEST_ROOT / name
        if not path.is_file():
            raise RuntimeError(f"missing competitive match contract: {name}")
        discovered[name] = path

    return [discovered[name] for name in sorted(discovered)]


def main() -> int:
    contracts = discover_contracts()
    for contract in contracts:
        print(f"competitive_match_contracts: RUN {contract.name}", flush=True)
        completed = subprocess.run(
            [sys.executable, str(contract)],
            cwd=ROOT,
            check=False,
        )
        if completed.returncode != 0:
            print(
                "competitive_match_contracts: FAIL "
                f"{contract.name} (exit {completed.returncode})",
                file=sys.stderr,
            )
            return completed.returncode

    print(f"competitive_match_contracts: PASS ({len(contracts)} contracts)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
