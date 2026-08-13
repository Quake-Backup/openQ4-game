#!/usr/bin/env python3
"""Static contract checks for high-refresh player/viewmodel presentation."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOTS = ("src/game", "src/mpgame")
PRESENTATION_STATE = (
    "presentationViewTime",
    "presentationCanInterpolate",
    "presentationPrevViewOrigin",
    "presentationPrevViewAxis",
    "presentationPrevFov",
    "presentationCurViewOrigin",
    "presentationCurViewAxis",
    "presentationCurFov",
)
PRESENTATION_WEAPON_STATE = (
    "presentationViewModelTime",
    "presentationViewModelCanInterpolate",
    "presentationPrevPlayerViewOrigin",
    "presentationPrevPlayerViewAxis",
    "presentationCurPlayerViewOrigin",
    "presentationCurPlayerViewAxis",
    "presentationPrevViewModelOrigin",
    "presentationPrevViewModelAxis",
    "presentationCurViewModelOrigin",
    "presentationCurViewModelAxis",
)


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def reject(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        raise AssertionError(f"Unexpected {needle!r} in {context}")


def function(source: str, signature: str, context: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"Missing function {signature!r} in {context}")
    opening = source.find("{", start + len(signature))
    if opening < 0:
        raise AssertionError(f"Missing body for {signature!r} in {context}")

    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1 : index]
    raise AssertionError(f"Unterminated body for {signature!r} in {context}")


def normalized(body: str) -> str:
    return " ".join(body.split())


def check_source_root(source_root: str) -> dict[str, str]:
    context = source_root
    game_local_h = read(f"{source_root}/Game_local.h")
    game_local_cpp = read(f"{source_root}/Game_local.cpp")
    multiplayer_cpp = read(f"{source_root}/MultiplayerGame.cpp")
    player_h = read(f"{source_root}/Player.h")
    player_cpp = read(f"{source_root}/Player.cpp")
    weapon_h = read(f"{source_root}/Weapon.h")
    weapon_cpp = read(f"{source_root}/Weapon.cpp")

    require(game_local_h, "mutable int\t\t\tpresentationClockGameTime", f"{context} transient clock")
    require(game_local_h, "GetPresentationInterpolationFraction", f"{context} interpolation API")

    clear = function(game_local_cpp, "void idGameLocal::Clear( void )", context)
    require(clear, "presentationClockGameTime = -1;", f"{context} clock reset")
    require(clear, "presentationClockRealTime = 0;", f"{context} clock reset")

    clock = function(
        game_local_cpp,
        "int idGameLocal::GetPresentationTimeMsec( void ) const",
        context,
    )
    require(clock, "GetDemoState() == DEMO_PLAYING || IsTimeDemo()", f"{context} demo clock bypass")
    require(clock, "const int realTime = Sys_Milliseconds();", f"{context} exported clock source")
    require(clock, "presentationClockGameTime != time", f"{context} simulation anchor")
    require(clock, "return time + Max( 0, realTime - presentationClockRealTime );", f"{context} mapped clock")
    reject(clock, "common->GetPresentationTime", f"{context} game-DLL clock boundary")

    fraction = function(
        game_local_cpp,
        "float idGameLocal::GetPresentationInterpolationFraction( void ) const",
        context,
    )
    require(fraction, "common->GetUserCmdMsecFloat()", f"{context} usercmd cadence")
    require(fraction, "idMath::ClampFloat( 0.0f, 1.0f", f"{context} bounded fraction")
    require(fraction, "bounded latency avoids extrapolating", f"{context} one-tic latency contract")

    axis = function(
        game_local_cpp,
        "idMat3 idGameLocal::InterpolatePresentationAxis",
        context,
    )
    require(axis, "blended.Slerp", f"{context} rotational interpolation")

    prepare = function(
        game_local_cpp,
        "void idGameLocal::PreparePlayerSceneForRender( idPlayer *player )",
        context,
    )
    require(prepare, "player->CalculateRenderView();", f"{context} draw-time camera refresh")
    require(prepare, "UpdatePresentationWeapon", f"{context} draw-time viewmodel refresh")
    reject(prepare, "Think(", f"{context} presentation-only draw pass")
    reject(prepare, "RunFrame(", f"{context} presentation-only draw pass")

    draw = function(game_local_cpp, "bool idGameLocal::Draw( int clientNum )", context)
    require(draw, "PreparePlayerSceneForRender( player );", f"{context} single-player draw path")
    multiplayer_draw = function(
        multiplayer_cpp,
        "bool idMultiplayerGame::Draw( int clientNum )",
        context,
    )
    require(
        multiplayer_draw,
        "gameLocal.PreparePlayerSceneForRender( viewPlayer );",
        f"{context} multiplayer/spectator draw path",
    )

    for field in PRESENTATION_STATE:
        require(player_h, field, f"{context} player presentation state")
    require(player_h, "GetPresentationViewPos", f"{context} player presentation API")

    reset = function(player_cpp, "void idPlayer::ResetPresentationViewState( void )", context)
    require(reset, "presentationViewTime = -1;", f"{context} reset")
    restore = function(player_cpp, "void idPlayer::Restore( idRestoreGame *savefile )", context)
    require(restore, "ResetPresentationViewState();", f"{context} restore reseed")
    save = function(player_cpp, "void idPlayer::Save( idSaveGame *savefile ) const", context)
    for field in PRESENTATION_STATE:
        reject(save, field, f"{context} save-format isolation")

    update = function(player_cpp, "void idPlayer::UpdatePresentationViewState( void )", context)
    require(update, "presentationViewTime == gameLocal.time", f"{context} zero-tic endpoint retention")
    require(update, "common->GetUserCmdMsecFloat()", f"{context} sequential-tic guard")
    require(update, "originDelta.LengthSqr() <= Square( 32.0f )", f"{context} teleport guard")
    require(update, "angleDelta.Length() <= 90.0f", f"{context} view discontinuity guard")
    require(update, "gameLocal.GetMHz() == common->GetUserCmdHz()", f"{context} exact cadence guard")
    if source_root == "src/mpgame":
        require(update, "!activePredictionViewSmoothing", f"{context} prediction smoothing guard")

    get_view = function(player_cpp, "void idPlayer::GetPresentationViewPos", context)
    require(get_view, "origin.Lerp", f"{context} position interpolation")
    require(get_view, "InterpolatePresentationAxis", f"{context} axis interpolation")

    render_view = function(player_cpp, "void idPlayer::CalculateRenderView( void )", context)
    require(render_view, "UpdatePresentationViewState();", f"{context} render-view endpoint capture")
    require(render_view, "GetPresentationViewPos", f"{context} first-person presentation pose")
    require(render_view, "GetPresentationFov()", f"{context} FOV interpolation")
    require(render_view, "renderView->time =", f"{context} presentation shader clock")
    reject(player_cpp, "GetPresentationViewDelta", f"{context} engine-only input sampling boundary")

    require(weapon_h, "UpdatePresentationWeapon", f"{context} viewmodel presentation API")
    for field in PRESENTATION_WEAPON_STATE:
        require(weapon_h, field, f"{context} transient viewmodel state")

    weapon_save = function(weapon_cpp, "void rvWeapon::Save ( idSaveGame *savefile ) const", context)
    for field in PRESENTATION_WEAPON_STATE:
        reject(weapon_save, field, f"{context} weapon save-format isolation")
    weapon_restore = function(weapon_cpp, "void rvWeapon::Restore ( idRestoreGame *savefile )", context)
    require(weapon_restore, "ResetPresentationViewModelState();", f"{context} weapon restore reseed")

    update_viewmodel = function(
        weapon_cpp,
        "void rvWeapon::UpdatePresentationViewModelState",
        context,
    )
    require(update_viewmodel, "presentationViewModelTime == gameLocal.time", f"{context} viewmodel endpoint retention")
    require(update_viewmodel, "owner->CanInterpolatePresentationView()", f"{context} camera/viewmodel cadence pairing")
    require(update_viewmodel, "originDelta.LengthSqr() <= Square( 24.0f )", f"{context} viewmodel discontinuity guard")
    require(update_viewmodel, "angleDelta.Length() <= 70.0f", f"{context} viewmodel angle guard")

    get_viewmodel = function(
        weapon_cpp,
        "void rvWeapon::GetPresentationViewModelTransform",
        context,
    )
    require(get_viewmodel, "owner->GetPresentationViewPos", f"{context} camera/viewmodel alignment")
    require(get_viewmodel, "prevLocalOrigin", f"{context} camera-local viewmodel interpolation")
    require(get_viewmodel, "localOrigin.Lerp", f"{context} complete viewmodel pose interpolation")
    require(get_viewmodel, "InterpolatePresentationAxis", f"{context} viewmodel axis interpolation")

    update_weapon = function(
        weapon_cpp,
        "void rvViewWeapon::UpdatePresentationWeapon( bool showViewModel )",
        context,
    )
    require(update_weapon, "const idVec3 authoritativeOrigin", f"{context} authoritative pose preservation")
    require(update_weapon, "ApplyPresentationViewModelTransform();", f"{context} interpolated viewmodel pose")
    require(update_weapon, "SetOrigin( authoritativeOrigin );", f"{context} authoritative pose restore")
    require(update_weapon, "SetAxis( authoritativeAxis );", f"{context} authoritative pose restore")
    reject(update_weapon, "weapon->Think", f"{context} no duplicate weapon logic")
    reject(update_weapon, "UpdateSound", f"{context} no duplicate sound update")

    update_model = function(
        weapon_cpp,
        "void rvViewWeapon::UpdatePresentationModel( void )",
        context,
    )
    require(update_model, "renderEntity_t presentationRenderEntity = renderEntity;", f"{context} transient render copy")
    require(update_model, "UpdateEntityDef", f"{context} viewmodel render resubmission")

    apply_transform = function(
        weapon_cpp,
        "void rvWeapon::ApplyPresentationViewModelTransform( void )",
        context,
    )
    require(apply_transform, "GetPresentationViewModelTransform", f"{context} cached viewmodel sample")

    think = function(weapon_cpp, "void rvWeapon::Think ( void )", context)
    require(think, "CalculateViewModelTransform", f"{context} authoritative viewmodel calculation")
    require(think, "UpdatePresentationViewModelState", f"{context} authoritative viewmodel endpoint capture")

    return {
        "clock": normalized(clock),
        "fraction": normalized(fraction),
        "axis": normalized(axis),
        "prepare": normalized(prepare),
        "reset": normalized(reset),
        "get_view": normalized(get_view),
        "update_viewmodel": normalized(update_viewmodel),
        "get_viewmodel": normalized(get_viewmodel),
        "update_weapon": normalized(update_weapon),
        "update_model": normalized(update_model),
        "apply_transform": normalized(apply_transform),
    }


def check_fraction_examples() -> None:
    def fraction(elapsed_msec: float, tic_msec: float) -> float:
        return max(0.0, min(1.0, elapsed_msec / tic_msec))

    assert fraction(-2.0, 16.0) == 0.0
    assert fraction(0.0, 16.0) == 0.0
    assert fraction(4.0, 16.0) == 0.25
    assert fraction(8.0, 16.0) == 0.5
    assert fraction(16.0, 16.0) == 1.0
    assert fraction(24.0, 16.0) == 1.0


def main() -> None:
    sp_contract = check_source_root(SOURCE_ROOTS[0])
    mp_contract = check_source_root(SOURCE_ROOTS[1])
    for method, sp_body in sp_contract.items():
        if sp_body != mp_contract[method]:
            raise AssertionError(f"SP/MP presentation method drift: {method}")
    check_fraction_examples()
    print("presentation_interpolation_contract: ok")


if __name__ == "__main__":
    main()
