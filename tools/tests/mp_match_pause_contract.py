#!/usr/bin/env python3
"""Static and executable contracts for competitive gameplay pause safety."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    path = ROOT / relative
    if not path.is_file():
        raise AssertionError(f"required source file not found: {path}")
    return path.read_text(encoding="utf-8", errors="strict")


def body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"missing {signature}")
    opening = source.find("{", start)
    if opening < 0:
        raise AssertionError(f"missing body for {signature}")
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1 : index]
    raise AssertionError(f"unterminated {signature}")


def require(text: str, token: str, context: str) -> None:
    if token not in text:
        raise AssertionError(f"missing {token!r} in {context}")


def reject(text: str, token: str, context: str) -> None:
    if token in text:
        raise AssertionError(f"unexpected {token!r} in {context}")


def require_before(text: str, first: str, second: str, context: str) -> None:
    first_at = text.find(first)
    second_at = text.find(second)
    if first_at < 0 or second_at < 0 or first_at >= second_at:
        raise AssertionError(
            f"expected {first!r} before {second!r} in {context}"
        )


def central_frame_contracts() -> None:
    game_local = read("src/mpgame/Game_local.cpp")
    game_network = read("src/mpgame/Game_network.cpp")
    multiplayer = read("src/mpgame/MultiplayerGame.cpp")
    events = read("src/mpgame/gamesys/Event.cpp")
    entity = read("src/mpgame/Entity.cpp")

    frame = body(game_local, "gameReturn_t idGameLocal::RunFrame")
    require_before(
        frame,
        "time += GetMSec();",
        "mpGame.BeginCompetitiveFrame();",
        "engine clock precedes authoritative pause boundary",
    )
    for later in (
        "random.RandomInt();",
        "botManager.Think();",
        "ServerProcessEntityNetworkEventQueue();",
        "// let entities think",
        "idEvent::ShiftEvents( msec )",
    ):
        require_before(
            frame,
            "mpGame.BeginCompetitiveFrame();",
            later,
            "pause commit precedes gameplay work",
        )

    entity_start = frame.find("// let entities think")
    entity_end = frame.find("// remove any entities that have stopped thinking")
    if entity_start < 0 or entity_end <= entity_start:
        raise AssertionError("could not isolate the entity-thinking section")
    entity_section = frame[entity_start:entity_end]
    frozen_start = entity_section.find("if ( competitiveGameplayFrozen ) {")
    frozen_end = entity_section.find("} else if", frozen_start)
    if frozen_start < 0 or frozen_end <= frozen_start:
        raise AssertionError("could not isolate the frozen entity pass")
    frozen = entity_section[frozen_start:frozen_end]
    require(frozen, "spawnedEntities.Next()", "complete frozen-owner traversal")
    require(frozen, "ent->ThinkMatchPaused( msec );", "deadline rebasing hook")
    reject(frozen, "activeEntities.Next()", "complete frozen-owner traversal")
    reject(frozen, "ent->Think();", "frozen gameplay simulation")
    reject(frozen, "RunPhysics", "frozen gameplay simulation")

    require(
        frame,
        "if ( !competitiveGameplayFrozen ) {\n\t\t\tServerProcessEntityNetworkEventQueue();",
        "network gameplay-event suppression",
    )
    require(
        frame,
        "if ( competitiveGameplayFrozen ) {\n\t\t\tif ( !idEvent::ShiftEvents( msec ) )",
        "posted-event rebasing branch",
    )
    require_before(
        frame,
        "idEvent::ShiftEvents( msec )",
        "mpGame.Run();",
        "gameplay deadlines before live match operations",
    )

    begin = body(multiplayer, "void idMultiplayerGame::BeginCompetitiveFrame")
    if begin.count("matchSession.AdvanceFrame(") != 1:
        raise AssertionError("the authoritative session clock must advance once per frame")
    if begin.count("RebaseCompetitivePauseFrame(") != 1:
        raise AssertionError("adapter-owned deadlines must rebase once per frozen frame")
    require_before(
        begin,
        "matchSession.AdvanceFrame(",
        "if ( IsGameplayFrozen() )",
        "session clock before pause overlay application",
    )

    session = read("src/mpgame/mp/match/MatchSession.cpp")
    running_clock = body(session, "bool mpMatchSession::IsMatchClockRunning")
    require(running_clock, "pause.state == MP_MATCH_PAUSE_RUNNING", "frozen match clock")
    reject(
        running_clock,
        "MP_MATCH_PAUSE_PENDING",
        "pending boundary frame must not advance match time",
    )

    shift_events = body(events, "bool idEvent::ShiftEvents")
    require(shift_events, "event->time > maxInt - deltaMsec", "overflow preflight")
    require_before(
        shift_events,
        "event->time > maxInt - deltaMsec",
        "event->time += deltaMsec",
        "transactional event-queue shift",
    )
    if shift_events.count("for ( idEvent *event = EventQueue.Next()") != 2:
        raise AssertionError("event shift must retain separate validation and commit passes")

    entity_pause = body(entity, "void idEntity::ThinkMatchPaused")
    require(entity_pause, "physics->GetTime()", "trajectory-local physics time source")
    require(entity_pause, "physics->UpdateTime", "mover trajectory rebase")
    reject(entity_pause, "physics->UpdateTime( gameLocal.time", "dormant trajectory time jump")
    require(entity_pause, "gameRenderWorld->UpdateEntityDef", "rebased shader parameters submitted")
    reject(entity_pause, "Evaluate(", "frozen collision evaluation")
    reject(entity_pause, "RunPhysics", "frozen collision evaluation")
    animated_pause = body(entity, "void idAnimatedEntity::ThinkMatchPaused")
    require(animated_pause, "animator.ShiftTime( deltaMsec );", "animation clock rebase")

    damage = body(entity, "void idEntity::Damage( idEntity *inflictor")
    require(damage, "IsGameplayFrozen()", "direct-damage suppression")
    radius = body(game_local, "void idGameLocal::RadiusDamage")
    require(radius, "IsGameplayFrozen()", "radius-damage suppression")

    prediction = body(game_network, "gameReturn_t idGameLocal::ClientPrediction")
    require_before(
        prediction,
        "time += GetMSec();",
        "const bool competitiveGameplayFrozen = mpGame.IsGameplayFrozen();",
        "client time precedes the projected pause boundary",
    )
    frozen_prediction = body(
        prediction, "if ( competitiveGameplayFrozen )"
    )
    new_frozen_frame = body(frozen_prediction, "if ( isNewFrame )")
    require(
        new_frozen_frame,
        "spawnedEntities.Next()",
        "complete client deadline-owner traversal",
    )
    require(
        new_frozen_frame,
        "ent->ThinkMatchPaused( frameMsec );",
        "client deadline rebasing hook",
    )
    require_before(
        new_frozen_frame,
        "usercmds = NULL;",
        "ent->ThinkMatchPaused( frameMsec );",
        "non-presenting deadline pass",
    )
    require_before(
        new_frozen_frame,
        "ent->ThinkMatchPaused( frameMsec );",
        "usercmds = savedUsercmds;",
        "restore prediction input after deadline pass",
    )
    require(
        new_frozen_frame,
        "idEvent::ShiftEvents( frameMsec )",
        "once-per-frame client posted-event rebase",
    )
    if frozen_prediction.count("ent->ThinkMatchPaused( frameMsec );") != 1:
        raise AssertionError("client entities must rebase exactly once per new frame")
    if frozen_prediction.count("idEvent::ShiftEvents( frameMsec )") != 1:
        raise AssertionError("client posted events must rebase exactly once per new frame")

    require(
        frozen_prediction,
        "pauseViewPlayer->ThinkMatchPaused( 0 );",
        "zero-delta view presentation on prediction replays",
    )
    reject(
        new_frozen_frame,
        "pauseViewPlayer->ThinkMatchPaused( 0 );",
        "view presentation must not be limited to real new frames",
    )
    for token in (
        "clientNum == MAX_CLIENTS && isRepeater",
        "GetDemoFollowClient()",
        "clientNum == MAX_CLIENTS && player && isNewFrame && !isRepeater",
        "followPlayer = player->spectator",
    ):
        require(frozen_prediction, token, "repeater/free-view pause routing")

    for forbidden in (
        "ClientPredictionThink();",
        "clientSpawnedEntities.Next()",
        "cent->Think();",
        "idEvent::ServiceEvents();",
        "bse->",
        "RunPhysics",
        "ent->Think();",
    ):
        reject(frozen_prediction, forbidden, "frozen client gameplay simulation")


def owned_deadline_contracts() -> None:
    projectile = read("src/mpgame/Projectile.cpp")
    mover = read("src/mpgame/Mover.cpp")
    moveable = read("src/mpgame/Moveable.cpp")
    actor = read("src/mpgame/Actor.cpp")
    item = read("src/mpgame/Item.cpp")
    trigger = read("src/mpgame/Trigger.cpp")
    light = read("src/mpgame/Light.cpp")
    state = read("src/mpgame/gamesys/State.cpp")
    player = read("src/mpgame/Player.cpp")
    weapon = read("src/mpgame/Weapon.cpp")
    game_state = read("src/mpgame/mp/GameState.cpp")
    round_state = read("src/mpgame/mp/RoundGameState.cpp")
    tourney = read("src/mpgame/mp/Tourney.cpp")
    round_modes = read("src/mpgame/mp/RoundModes.cpp")

    projectile_pause = body(projectile, "void idProjectile::ThinkMatchPaused")
    for token in (
        "speed.SetStartTime",
        "rotation.SetStartTime",
        "mpMatchShiftTimeOrigin( launchTime",
        "mpMatchShiftOptionalDeadline( lightEndTime",
        "gameRenderWorld->UpdateLightDef",
    ):
        require(projectile_pause, token, "projectile pause clocks")
    guided_pause = body(projectile, "void idGuidedProjectile::ThinkMatchPaused")
    for token in ("launchTime", "driftTime", "turn_max.SetStartTime"):
        require(guided_pause, token, "guided-projectile pause clocks")
    drifting_pause = body(projectile, "void rvDriftingProjectile::ThinkMatchPaused")
    require(drifting_pause, "driftOffset", "drifting-projectile offset clocks")
    require(drifting_pause, "driftSpeed.SetStartTime", "drifting-projectile speed clock")

    mover_pause = body(mover, "void idMover::ThinkMatchPaused")
    require(mover_pause, "splineStartTime", "spline mover phase clock")
    require(mover_pause, "splineStateThread.ShiftMatchTime", "spline mover state delays")
    binary_pause = body(mover, "void idMover_Binary::ThinkMatchPaused")
    require(binary_pause, "stateStartTime", "binary mover interpolation origin")
    moveable_pause = body(moveable, "void idMoveable::ThinkMatchPaused")
    require(moveable_pause, "initialSpline->ShiftTime", "moveable initial spline")
    barrel_pause = body(moveable, "void idExplodingBarrel::ThinkMatchPaused")
    for token in ("explodeFinishTime", "UpdateEntityDef"):
        require(barrel_pause, token, "exploding-moveable clocks")

    actor_pause = body(actor, "void idActor::ThinkMatchPaused")
    for token in (
        "pain_debounce_time",
        "deathPushTime",
        "stateThread.ShiftMatchTime",
        "headAnim.GetStateThread().ShiftMatchTime",
        "torsoAnim.GetStateThread().ShiftMatchTime",
        "legsAnim.GetStateThread().ShiftMatchTime",
    ):
        require(actor_pause, token, "actor gameplay clocks")

    state_pause = body(state, "void rvStateThread::ShiftMatchTime")
    require(state_pause, "states.Next()", "queued state-delay clocks")
    require(state_pause, "interrupted.Next()", "interrupted state-delay clocks")
    if state_pause.count("mpMatchShiftOptionalDeadline( call->parms.time") != 2:
        raise AssertionError("both state-thread queues must shift their delay origins")

    item_pause = body(item, "void idItemPowerup::ThinkMatchPaused")
    require(item_pause, "droppedTime", "dropped-powerup remaining lifetime")
    for signature, field in (
        ("void idTrigger_Multi::ThinkMatchPaused", "nextTriggerTime"),
        ("void idTrigger_EntityName::ThinkMatchPaused", "nextTriggerTime"),
        ("void idTrigger_Hurt::ThinkMatchPaused", "nextTime"),
    ):
        require(body(trigger, signature), field, "trigger cooldown clocks")

    light_pause = body(light, "void idLight::ThinkMatchPaused")
    require(light_pause, "fadeStart", "light fade origin")
    require(light_pause, "fadeEnd", "light fade deadline")
    require(light_pause, "PresentLightDefChange();", "paused light render submission")

    player_pause = body(player, "void idPlayer::ThinkMatchPaused")
    require(player_pause, "inventory.ShiftMatchTime", "powerup and regeneration clocks")
    require(player_pause, "weapon->ShiftMatchTime", "weapon gameplay clocks")
    require(player_pause, "lastArenaChange", "Tourney input cooldown")
    require(player_pause, "lastSpectateTeleport", "spectator teleport cooldown")
    weapon_pause = body(weapon, "void rvWeapon::ShiftMatchTime")
    require(weapon_pause, "stateThread.ShiftMatchTime", "weapon state-delay clocks")

    base_state_pause = body(game_state, "void rvGameState::ShiftMatchTime")
    for token in ("nextStateTime", "fragLimitTimeout", "overtimeStartTime"):
        require(base_state_pause, token, "base match-state deadlines")
    round_pause = body(round_state, "void rvRoundGameState::ShiftMatchTime")
    for token in ("roundStartTime", "roundStateTime"):
        require(round_pause, token, "round deadlines")
    tourney_pause = body(tourney, "void rvTourneyArena::ShiftMatchTime")
    for token in ("nextStateTime", "fragLimitTimeout", "matchStartTime"):
        require(tourney_pause, token, "Tourney arena deadlines")
    freeze_pause = body(round_modes, "void rvFreezeTagGameState::ShiftMatchTime")
    require(freeze_pause, "lastThawAnnounce", "Freeze Tag notification cadence")


def compile_deadline_harness() -> None:
    compiler = next(
        (path for name in ("c++", "g++", "clang++", "cl") if (path := shutil.which(name))),
        None,
    )
    if compiler is None:
        print("mp_match_pause_contract: no standalone C++ compiler; static checks only")
        return

    tmp_root = ROOT / ".tmp"
    tmp_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mp-pause-contract-", dir=tmp_root) as raw:
        temp = Path(raw)
        source = temp / "deadline_harness.cpp"
        source.write_text(
            r'''
#include "mp/match/MatchDeadline.h"

int main() {
    if ( mpMatchAddMillisecondsClamped( 2147483640, 16 ) != 2147483647 ) return 1;
    if ( mpMatchAddMillisecondsClamped( 10, 16 ) != 26 ) return 2;

    int optional = 0;
    mpMatchShiftOptionalDeadline( optional, 16 );
    if ( optional != 0 ) return 3;
    optional = 10;
    mpMatchShiftOptionalDeadline( optional, 16 );
    if ( optional != 26 ) return 4;

    int origin = 0;
    mpMatchShiftTimeOrigin( origin, 16 );
    if ( origin != 16 ) return 5;
    origin = -1;
    mpMatchShiftTimeOrigin( origin, 16 );
    if ( origin != -1 ) return 6;

    float floatOrigin = 0.0f;
    mpMatchShiftTimeOrigin( floatOrigin, 16 );
    if ( floatOrigin != 16.0f ) return 7;
    return 0;
}
'''.lstrip(),
            encoding="utf-8",
        )

        compiler_name = Path(compiler).name.lower()
        if compiler_name in ("cl", "cl.exe"):
            executable = temp / "deadline_harness.exe"
            command = [
                compiler,
                "/nologo",
                "/EHsc",
                f"/I{ROOT / 'src/mpgame'}",
                str(source),
                f"/Fe:{executable}",
            ]
        else:
            executable = temp / "deadline_harness"
            command = [
                compiler,
                "-std=c++11",
                "-Wall",
                "-Wextra",
                "-pedantic",
                f"-I{ROOT / 'src/mpgame'}",
                str(source),
                "-o",
                str(executable),
            ]
        subprocess.run(command, cwd=temp, check=True)
        subprocess.run([str(executable)], cwd=temp, check=True)


def main() -> None:
    central_frame_contracts()
    owned_deadline_contracts()
    compile_deadline_harness()
    print("mp_match_pause_contract: ok")


if __name__ == "__main__":
    main()
