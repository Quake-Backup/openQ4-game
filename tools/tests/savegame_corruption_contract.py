#!/usr/bin/env python3
"""Static savegame corruption guardrails for openQ4 GameLibs."""

from __future__ import annotations

import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GAME_TREES = ("game", "mpgame")
RESTORE_GUARD_ERROR_PATTERN = re.compile(
    r'(?:savefile|saveFile|savegame)\s*(?:->|\.)\s*Error\s*\(\s*"([^"]+)"'
)
RAW_SAVE_WRITE_PATTERN = re.compile(
    r"\b(?:savefile|saveFile|savegame)\s*(?:->|\.)\s*Write\s*\("
)
EXPECTED_SAVEGAME_CAPS = {
    "MAX_SAVEGAME_ENTITY_REFS": 1024,
    "MAX_SAVEGAME_CLIENT_ENTITY_REFS": 1024,
    "MAX_SAVEGAME_BRITTLE_SHARDS": 1024,
    "MAX_SAVEGAME_BRITTLE_DECALS_PER_SHARD": 64,
    "MAX_SAVEGAME_BRITTLE_EDGES_PER_SHARD": 64,
    "MAX_SAVEGAME_AI_ANIM_INFOS": 256,
    "MAX_SAVEGAME_AI_LOOK_JOINTS": 128,
    "MAX_SAVEGAME_AI_ACTION_ANIMS": 256,
    "MAX_SAVEGAME_AI_TRIGGER_TESTS": 256,
    "MAX_SAVEGAME_STATE_CALLS": 256,
    "MAX_SAVEGAME_SCRIPT_FUNC_PARMS": 64,
    "MAX_SAVEGAME_INVENTORY_ITEMS": 512,
    "MAX_SAVEGAME_LEVEL_TRIGGERS": 256,
    "MAX_SAVEGAME_PICKUP_ITEMS": 512,
    "MAX_SAVEGAME_OBJECTIVES": 256,
    "MAX_SAVEGAME_AAS_LOCATIONS": 128,
    "MAX_SAVEGAME_GUI_EVENTS": 256,
    "MAX_SAVEGAME_SPAWNER_ACTIVE": 512,
    "MAX_SAVEGAME_SPAWNER_POINTS": 512,
    "MAX_SAVEGAME_SPAWNER_CALLBACKS": 256,
    "MAX_SAVEGAME_VEHICLE_POSITIONS": 64,
    "MAX_SAVEGAME_VEHICLE_PARTS": 256,
    "MAX_SAVEGAME_VEHICLE_WEAPONS": 128,
    "MAX_SAVEGAME_VEHICLE_WEAPON_JOINTS": 128,
    "MAX_SAVEGAME_TRAM_OCCUPANTS": 64,
    "MAX_SAVEGAME_TRAM_WEAPONS": 32,
    "MAX_SAVEGAME_TRAM_VISIBLE_ENEMIES": 256,
    "MAX_SAVEGAME_TRAM_GATE_DOORS": 32,
    "MAX_SAVEGAME_TRIGGER_FUNCTIONS": 256,
    "MAX_SAVEGAME_MOVER_BUDDIES": 128,
    "MAX_SAVEGAME_MOVER_GUI_TARGETS": 128,
    "MAX_SAVEGAME_SHAKING_TARGET_HISTORY": 256,
    "MAX_SAVEGAME_CHAIN_LIGHTNING_TARGETS": 64,
    "MAX_SAVEGAME_ROCKET_GUIDE_ENTS": 128,
    "MAX_SAVEGAME_CLIENT_CRAWL_JOINTS": 128,
    "MAX_SAVEGAME_PHYSICS_CONTACTS": 256,
    "MAX_SAVEGAME_PHYSICS_CONTACT_ENTITIES": 256,
    "MAX_SAVEGAME_STATIC_MULTI_STATES": 256,
    "MAX_SAVEGAME_STATIC_MULTI_CLIP_MODELS": 256,
    "MAX_SAVEGAME_TARGET_INFLUENCE_ITEMS": 1024,
    "MAX_SAVEGAME_ACTOR_ENEMIES": 512,
    "MAX_SAVEGAME_ACTOR_ATTACHMENTS": 256,
}

EXPECTED_RAW_SAVE_WRITE_COUNTS = {
    "BrittleFracture.cpp": 1,
    "SecurityCamera.cpp": 1,
    "ai/Monster_ConvoyHover.cpp": 2,
    "anim/Anim_Blend.cpp": 5,
    "physics/Clip.cpp": 1,
    "physics/Physics_AF.cpp": 2,
    "script/Script_Interpreter.cpp": 1,
    "script/Script_Program.cpp": 1,
    "vehicle/Vehicle.cpp": 1,
    "vehicle/VehicleDriver.cpp": 2,
    "vehicle/VehicleParts.cpp": 1,
    "vehicle/VehiclePosition.cpp": 5,
}


def read(relative_path: str) -> str:
    path = ROOT / relative_path
    if not path.is_file():
        raise AssertionError(f"Required source file not found: {path}")
    return path.read_text(encoding="utf-8")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def require_regex(haystack: str, pattern: str, context: str) -> None:
    if re.search(pattern, haystack, re.MULTILINE | re.DOTALL) is None:
        raise AssertionError(f"Missing pattern {pattern!r} in {context}")


def reject_regex(haystack: str, pattern: str, context: str) -> None:
    match = re.search(pattern, haystack, re.MULTILINE | re.DOTALL)
    if match is not None:
        raise AssertionError(f"Unexpected pattern {pattern!r} in {context}: {match.group(0)!r}")


def parse_last_define_int(relative_path: str, name: str) -> int:
    values = re.findall(rf"^\s*#define\s+{re.escape(name)}\s+(\d+)\b", read(relative_path), re.MULTILINE)
    if not values:
        raise AssertionError(f"Missing integer #define for {name} in {relative_path}")
    return int(values[-1])


def parse_int_constant(relative_path: str, pattern: str, context: str) -> int:
    match = re.search(pattern, read(relative_path), re.MULTILINE)
    if match is None:
        raise AssertionError(f"Missing integer constant for {context}")
    return int(match.group(1))


class CorruptSave(ValueError):
    pass


MAX_CLIENTS = parse_last_define_int("src/game/Game_local.h", "MAX_CLIENTS")
MAX_GENTITIES = 1 << parse_last_define_int("src/game/Game_local.h", "GENTITYNUM_BITS")
MAX_CENTITIES = 1 << parse_last_define_int("src/game/Game_local.h", "CENTITYNUM_BITS")
MAX_SAVEGAME_OBJECTS = MAX_GENTITIES + MAX_CENTITIES + 4096
MAX_SAVEGAME_DICT_ENTRIES = parse_int_constant(
    "src/game/gamesys/SaveGame.cpp",
    r"MAX_SAVEGAME_DICT_ENTRIES\s*=\s*(\d+)",
    "MAX_SAVEGAME_DICT_ENTRIES",
)
MAX_PRINT_MSG = parse_int_constant(
    "src/game/gamesys/SaveGame.cpp",
    r"#define\s+MAX_PRINT_MSG\s+(\d+)",
    "MAX_PRINT_MSG",
)
MAX_POINTS_ON_WINDING = parse_int_constant(
    "src/idlib/geometry/Winding.h",
    r"#define\s+MAX_POINTS_ON_WINDING\s+(\d+)",
    "MAX_POINTS_ON_WINDING",
)


class SavegameGuardReader:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.offset = 0

    def read_int(self) -> int:
        if self.offset + 4 > len(self.data):
            raise CorruptSave("truncated int")
        value = struct.unpack_from("<i", self.data, self.offset)[0]
        self.offset += 4
        return value

    def read_count(self, label: str, max_count: int, min_count: int = 0) -> int:
        count = self.read_int()
        if count < min_count or count > max_count:
            raise CorruptSave(f"invalid {label} {count} (range {min_count}..{max_count})")
        return count

    def read_index(self, label: str, count: int) -> int:
        index = self.read_int()
        if index < 0 or index >= count:
            raise CorruptSave(f"invalid {label} {index} (count {count})")
        return index

    def read_string(self) -> bytes:
        length = self.read_count("string length", MAX_PRINT_MSG - 1)
        remaining = len(self.data) - self.offset
        if length > remaining:
            raise CorruptSave(f"truncated string length {length}")
        value = self.data[self.offset : self.offset + length]
        self.offset += length
        return value

    def read_dict_count(self) -> int:
        count = self.read_int()
        if count == -1:
            return count
        if count < -1 or count > MAX_SAVEGAME_DICT_ENTRIES:
            raise CorruptSave(f"invalid dict count {count}")
        return count

    def require_eof(self) -> None:
        if self.offset != len(self.data):
            raise CorruptSave("trailing bytes")


def int32(value: int) -> bytes:
    return struct.pack("<i", value)


def expect_corrupt(data: bytes, parser, label: str) -> None:
    try:
        parser(data)
    except CorruptSave:
        return
    raise AssertionError(f"{label} unexpectedly parsed as valid")


def parse_object_count(data: bytes) -> None:
    reader = SavegameGuardReader(data)
    reader.read_count("object count", MAX_SAVEGAME_OBJECTS)
    reader.require_eof()


def parse_object_index(data: bytes, object_count: int = 3) -> None:
    reader = SavegameGuardReader(data)
    reader.read_index("object index", object_count)
    reader.require_eof()


def parse_dict_count(data: bytes) -> None:
    reader = SavegameGuardReader(data)
    reader.read_dict_count()
    reader.require_eof()


def parse_restore_string(data: bytes) -> None:
    reader = SavegameGuardReader(data)
    reader.read_string()
    reader.require_eof()


def parse_winding_point_count(data: bytes) -> None:
    reader = SavegameGuardReader(data)
    reader.read_count("winding point count", MAX_POINTS_ON_WINDING)
    reader.require_eof()


def parse_count_guard(data: bytes, max_count: int, label: str) -> None:
    reader = SavegameGuardReader(data)
    reader.read_count(label, max_count)
    reader.require_eof()


def parse_entity_number(data: bytes) -> None:
    reader = SavegameGuardReader(data)
    reader.read_index("entity number", MAX_GENTITIES)
    reader.require_eof()


def parse_first_free_entity_index(data: bytes) -> None:
    reader = SavegameGuardReader(data)
    reader.read_index("first free entity index", MAX_GENTITIES)
    reader.require_eof()


def parse_brittle_shard_guard(data: bytes) -> None:
    reader = SavegameGuardReader(data)
    shard_count = reader.read_count("brittle shard count", EXPECTED_SAVEGAME_CAPS["MAX_SAVEGAME_BRITTLE_SHARDS"])
    for _ in range(shard_count):
        edge_count = reader.read_count(
            "brittle winding point count",
            EXPECTED_SAVEGAME_CAPS["MAX_SAVEGAME_BRITTLE_EDGES_PER_SHARD"],
        )
        reader.read_count("brittle decal count", EXPECTED_SAVEGAME_CAPS["MAX_SAVEGAME_BRITTLE_DECALS_PER_SHARD"])
        reader.read_count("brittle neighbour count", edge_count)
        edge_neighbour_count = reader.read_int()
        if edge_neighbour_count != edge_count:
            raise CorruptSave(f"invalid brittle edge-neighbour count {edge_neighbour_count} for {edge_count} edges")
    reader.require_eof()


def normalize_guard_text(text: str) -> str:
    return " ".join(text.replace("saveFile", "savefile").replace("savegame.", "savefile->").split())


def extract_if_condition(blob: str) -> str:
    matches = list(re.finditer(r"\bif\s*\(", blob))
    if not matches:
        return "<no-if>"

    open_paren = blob.find("(", matches[-1].start())
    depth = 0
    condition_start = open_paren + 1
    for index in range(open_paren, len(blob)):
        char = blob[index]
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return normalize_guard_text(blob[condition_start:index])

    return "<unterminated-if>"


def extract_restore_guard_signatures(tree: str) -> set[tuple[str, str, str]]:
    signatures: set[tuple[str, str, str]] = set()

    for path in sorted((ROOT / "src" / tree).rglob("*.cpp")):
        lines = path.read_text(encoding="utf-8").splitlines()
        relative_path = str(path.relative_to(ROOT / "src" / tree)).replace("\\", "/")

        for line_index, line in enumerate(lines):
            if "Error" not in line or ("invalid" not in line and "unexpected end" not in line):
                continue

            error_match = RESTORE_GUARD_ERROR_PATTERN.search(line)
            if error_match is None:
                continue

            guard_start = line_index
            for candidate in range(line_index, max(-1, line_index - 12), -1):
                if re.search(r"\bif\s*\(", lines[candidate]) is not None:
                    guard_start = candidate
                    break

            guard_blob = " ".join(lines[guard_start : line_index + 1])
            signatures.add(
                (
                    relative_path,
                    extract_if_condition(guard_blob),
                    error_match.group(1),
                )
            )

    return signatures


def validate_restore_fuzz_model() -> None:
    parse_object_count(int32(0))
    expect_corrupt(b"\x00\x01", parse_object_count, "truncated object count")
    expect_corrupt(int32(-1), parse_object_count, "negative object count")
    expect_corrupt(int32(MAX_SAVEGAME_OBJECTS + 1), parse_object_count, "oversized object count")

    parse_object_index(int32(0))
    parse_object_index(int32(2))
    expect_corrupt(int32(-1), parse_object_index, "negative object index")
    expect_corrupt(int32(3), parse_object_index, "oversized object index")

    for valid_dict_count in (-1, 0, MAX_SAVEGAME_DICT_ENTRIES):
        parse_dict_count(int32(valid_dict_count))
    expect_corrupt(int32(-2), parse_dict_count, "dictionary count below clear sentinel")
    expect_corrupt(int32(MAX_SAVEGAME_DICT_ENTRIES + 1), parse_dict_count, "oversized dictionary count")

    parse_restore_string(int32(0))
    parse_restore_string(int32(3) + b"abc")
    expect_corrupt(int32(-1), parse_restore_string, "negative restore string length")
    expect_corrupt(int32(MAX_PRINT_MSG), parse_restore_string, "oversized restore string length")
    expect_corrupt(int32(4) + b"abc", parse_restore_string, "truncated restore string")

    for valid_winding_points in (0, MAX_POINTS_ON_WINDING):
        parse_winding_point_count(int32(valid_winding_points))
    expect_corrupt(int32(-1), parse_winding_point_count, "negative winding point count")
    expect_corrupt(int32(MAX_POINTS_ON_WINDING + 1), parse_winding_point_count, "oversized winding point count")

    fixed_count_fields = (
        ("client count", MAX_CLIENTS),
        ("entity count", MAX_GENTITIES),
        ("spawned entity count", MAX_GENTITIES),
        ("script object proxy count", MAX_GENTITIES + MAX_CENTITIES),
        ("client spawned entity count", MAX_CENTITIES),
        ("active entity count", MAX_GENTITIES),
        ("deactivation count", MAX_GENTITIES),
        ("entity number", MAX_GENTITIES - 1),
        ("first free entity index", MAX_GENTITIES - 1),
    )
    for label, max_count in fixed_count_fields:
        if label == "entity number":
            parser = parse_entity_number
        elif label == "first free entity index":
            parser = parse_first_free_entity_index
        else:
            parser = lambda data, max_count=max_count, label=label: parse_count_guard(data, max_count, label)
        parser(int32(0))
        parser(int32(max_count))
        expect_corrupt(int32(-1), parser, f"negative {label}")
        expect_corrupt(int32(max_count + 1), parser, f"oversized {label}")

    domain_count_fields = (
        ("entity target count", "MAX_SAVEGAME_ENTITY_REFS"),
        ("entity bound client entity count", "MAX_SAVEGAME_CLIENT_ENTITY_REFS"),
        ("entity signal count", "MAX_SAVEGAME_ENTITY_REFS"),
        ("brittle shard count", "MAX_SAVEGAME_BRITTLE_SHARDS"),
        ("brittle decal count", "MAX_SAVEGAME_BRITTLE_DECALS_PER_SHARD"),
        ("brittle winding point count", "MAX_SAVEGAME_BRITTLE_EDGES_PER_SHARD"),
        ("AI animation info count", "MAX_SAVEGAME_AI_ANIM_INFOS"),
        ("AI look joint count", "MAX_SAVEGAME_AI_LOOK_JOINTS"),
        ("AI action animation count", "MAX_SAVEGAME_AI_ACTION_ANIMS"),
        ("AI trigger test count", "MAX_SAVEGAME_AI_TRIGGER_TESTS"),
        ("state call count", "MAX_SAVEGAME_STATE_CALLS"),
        ("script function parameter count", "MAX_SAVEGAME_SCRIPT_FUNC_PARMS"),
        ("inventory item count", "MAX_SAVEGAME_INVENTORY_ITEMS"),
        ("level trigger count", "MAX_SAVEGAME_LEVEL_TRIGGERS"),
        ("pickup item count", "MAX_SAVEGAME_PICKUP_ITEMS"),
        ("objective count", "MAX_SAVEGAME_OBJECTIVES"),
        ("AAS location count", "MAX_SAVEGAME_AAS_LOCATIONS"),
        ("GUI event count", "MAX_SAVEGAME_GUI_EVENTS"),
        ("spawner active count", "MAX_SAVEGAME_SPAWNER_ACTIVE"),
        ("spawner point count", "MAX_SAVEGAME_SPAWNER_POINTS"),
        ("spawner callback count", "MAX_SAVEGAME_SPAWNER_CALLBACKS"),
        ("vehicle position count", "MAX_SAVEGAME_VEHICLE_POSITIONS"),
        ("vehicle part count", "MAX_SAVEGAME_VEHICLE_PARTS"),
        ("vehicle weapon count", "MAX_SAVEGAME_VEHICLE_WEAPONS"),
        ("vehicle weapon joint count", "MAX_SAVEGAME_VEHICLE_WEAPON_JOINTS"),
        ("tram occupant count", "MAX_SAVEGAME_TRAM_OCCUPANTS"),
        ("tram weapon count", "MAX_SAVEGAME_TRAM_WEAPONS"),
        ("tram visible enemy count", "MAX_SAVEGAME_TRAM_VISIBLE_ENEMIES"),
        ("tram gate door count", "MAX_SAVEGAME_TRAM_GATE_DOORS"),
        ("trigger function count", "MAX_SAVEGAME_TRIGGER_FUNCTIONS"),
        ("mover buddy count", "MAX_SAVEGAME_MOVER_BUDDIES"),
        ("mover GUI target count", "MAX_SAVEGAME_MOVER_GUI_TARGETS"),
        ("shaking target history count", "MAX_SAVEGAME_SHAKING_TARGET_HISTORY"),
        ("chain lightning target count", "MAX_SAVEGAME_CHAIN_LIGHTNING_TARGETS"),
        ("rocket guide entity count", "MAX_SAVEGAME_ROCKET_GUIDE_ENTS"),
        ("client crawl joint count", "MAX_SAVEGAME_CLIENT_CRAWL_JOINTS"),
        ("physics contact count", "MAX_SAVEGAME_PHYSICS_CONTACTS"),
        ("physics contact entity count", "MAX_SAVEGAME_PHYSICS_CONTACT_ENTITIES"),
        ("static multi state count", "MAX_SAVEGAME_STATIC_MULTI_STATES"),
        ("static multi clip model count", "MAX_SAVEGAME_STATIC_MULTI_CLIP_MODELS"),
        ("target influence item count", "MAX_SAVEGAME_TARGET_INFLUENCE_ITEMS"),
        ("actor enemy count", "MAX_SAVEGAME_ACTOR_ENEMIES"),
        ("actor attachment count", "MAX_SAVEGAME_ACTOR_ATTACHMENTS"),
    )
    for label, cap_name in domain_count_fields:
        max_count = EXPECTED_SAVEGAME_CAPS[cap_name]
        parser = lambda data, max_count=max_count, label=label: parse_count_guard(data, max_count, label)
        parser(int32(0))
        parser(int32(max_count))
        expect_corrupt(int32(-1), parser, f"negative {label}")
        expect_corrupt(int32(max_count + 1), parser, f"oversized {label}")

    parse_brittle_shard_guard(int32(0))
    parse_brittle_shard_guard(int32(1) + int32(3) + int32(0) + int32(3) + int32(3))
    expect_corrupt(
        int32(1) + int32(-1) + int32(0) + int32(0) + int32(0),
        parse_brittle_shard_guard,
        "negative brittle winding point count",
    )
    expect_corrupt(
        int32(1)
        + int32(EXPECTED_SAVEGAME_CAPS["MAX_SAVEGAME_BRITTLE_EDGES_PER_SHARD"] + 1)
        + int32(0)
        + int32(0)
        + int32(0),
        parse_brittle_shard_guard,
        "oversized brittle winding point count",
    )
    expect_corrupt(
        int32(1) + int32(3) + int32(-1) + int32(0) + int32(3),
        parse_brittle_shard_guard,
        "negative brittle decal count",
    )
    expect_corrupt(
        int32(1)
        + int32(3)
        + int32(EXPECTED_SAVEGAME_CAPS["MAX_SAVEGAME_BRITTLE_DECALS_PER_SHARD"] + 1)
        + int32(0)
        + int32(3),
        parse_brittle_shard_guard,
        "oversized brittle decal count",
    )
    expect_corrupt(
        int32(1) + int32(3) + int32(0) + int32(-1) + int32(3),
        parse_brittle_shard_guard,
        "negative brittle neighbour count",
    )
    expect_corrupt(
        int32(1) + int32(3) + int32(0) + int32(4) + int32(3),
        parse_brittle_shard_guard,
        "oversized brittle neighbour count",
    )
    expect_corrupt(
        int32(1) + int32(3) + int32(0) + int32(3) + int32(2),
        parse_brittle_shard_guard,
        "mismatched brittle edge-neighbour count",
    )


def validate_domain_caps(tree: str) -> None:
    source = read(f"src/{tree}/Game_local.h")

    for name, value in EXPECTED_SAVEGAME_CAPS.items():
        require_regex(source, rf"const\s+int\s+{name}\s*=\s*{value}\s*;", f"{tree} savegame cap {name}")
        reject_regex(source, rf"const\s+int\s+{name}\s*=\s*MAX_GENTITIES\b", f"{tree} savegame cap {name}")


def validate_restore_io_guards(tree: str) -> None:
    source = read(f"src/{tree}/gamesys/SaveGame.cpp")

    for token in (
        "static const int MAX_SAVEGAME_OBJECTS = MAX_GENTITIES + MAX_CENTITIES + 4096;",
        "static const int MAX_SAVEGAME_DICT_ENTRIES = 16384;",
        "class idScopedSaveMemoryFile",
        "~idScopedSaveMemoryFile( void )",
        "fileSystem->CloseFile( file );",
        "idScopedSaveMemoryFile memoryFile;",
        "idFile\t\t*mp = memoryFile.GetFile();",
        "sg.Close();",
        "void idRestoreGame::ReadChecked( void *buffer, int len, const char *detail )",
        "if ( bytesRead != len )",
        "unexpected end of savegame",
        "if ( num < 0 || num > MAX_SAVEGAME_OBJECTS )",
        "invalid object count",
        "if ( len < 0 || len >= MAX_PRINT_MSG )",
        "idRestoreGame::ReadString: invalid length",
        "if ( ( index < 0 ) || ( index >= objects.Num() ) )",
        "idRestoreGame::ReadObject: invalid object index",
        "if ( num < -1 || num > MAX_SAVEGAME_DICT_ENTRIES )",
        "idRestoreGame::ReadDict: invalid key/value count",
        "if ( num < 0 || num > MAX_POINTS_ON_WINDING )",
        "idRestoreGame::ReadWinding: invalid point count",
    ):
        require(source, token, f"{tree} savegame low-level corruption guard")

    reject_regex(
        source,
        r"fileSystem->CloseFile\(\s*mp\s*\)\s*;",
        f"{tree} checkSave memory file must be scoped",
    )


def validate_object_reference_guards(tree: str) -> None:
    source = read(f"src/{tree}/gamesys/SaveGame.cpp")
    header = read(f"src/{tree}/gamesys/SaveGame.h")
    event_source = read(f"src/{tree}/gamesys/Event.cpp")
    game_local = read(f"src/{tree}/Game_local.cpp")

    for token in (
        "ReadObject( idClass *&obj, const idTypeInfo &expectedType, const char *detail )",
        "const idTypeInfo &expectedType = T::GetClassType();",
        "ReadObject( restoredObject, expectedType, expectedType.classname );",
    ):
        require(header, token, f"{tree} typed restored-object API")

    for token in (
        "class idScopedUnrestoredObjectCleanup",
        "for ( int i = 1; i < objectList.Num(); i++ )",
        "objectList.Clear();",
        "SaveGame_DeleteUnrestoredObjects( objects );",
        "idScopedUnrestoredObjectCleanup cleanup( objects );",
        "cleanup.Release();",
        "WriteObject FindIndex failed; writing NULL reference",
        "if ( objects.Num() == 0 )",
        "if ( index != 0 && obj == NULL )",
        "unresolved object index",
        "if ( obj != NULL && !obj->IsType( expectedType ) )",
        "has type '%s', expected '%s'",
    ):
        require(source, token, f"{tree} restored-object integrity guard")

    destructor_start = source.index("idSaveGame::~idSaveGame( void )")
    destructor_end = source.index("/*", destructor_start)
    destructor_block = source[destructor_start:destructor_end]
    reject_regex(
        destructor_block,
        r"Close\s*\(\s*\)\s*;",
        f"{tree} save writer destructor must not serialize during error unwinding",
    )

    write_object_start = source.index("void idSaveGame::WriteObject( const idClass *obj )")
    write_object_end = source.index("/*", write_object_start)
    write_object_block = source[write_object_start:write_object_end]
    for token in (
        "SaveGame_FindObjectIndex( objects, objectHash, obj )",
        "WriteObject FindIndex failed; writing NULL reference",
        "index = 0;",
        "WriteInt( index );",
    ):
        require(write_object_block, token, f"{tree} transient saved-object fallback")
    reject_regex(
        write_object_block,
        r"gameLocal\.Error|obj->GetClassname\s*\(",
        f"{tree} transient saved-object fallback must not abort or dereference the object",
    )
    require(game_local, "savegame.Close();", f"{tree} primary save explicitly closes its writer")
    require(source, "sg.Close();", f"{tree} checkSave explicitly closes its writer")
    require_regex(
        source,
        r"objectTypes\.SetNum\s*\(\s*num\s*\+\s*1\s*\)\s*;.*?"
        r"for\s*\(\s*i\s*=\s*1\s*;\s*i\s*<\s*objectTypes\.Num\s*\(\s*\)\s*;\s*i\+\+\s*\).*?"
        r"objectTypes\s*\[\s*i\s*\]\s*=\s*type\s*;.*?"
        r"objects\.SetNum\s*\(\s*num\s*\+\s*1\s*\)",
        f"{tree} complete object type table is validated before allocation",
    )
    for token in (
        "savefile->ReadObject( event->object, *event->typeinfo, event->eventdef->GetName() );",
        "if ( event->object == NULL )",
        "has a NULL target object",
    ):
        require(event_source, token, f"{tree} restored event target validation")

    unsafe_object_cast = r"reinterpret_cast\s*<\s*idClass\s*\*&\s*>"
    for pattern in ("*.cpp", "*.h"):
        for path in sorted((ROOT / "src" / tree).rglob(pattern)):
            reject_regex(
                path.read_text(encoding="utf-8"),
                unsafe_object_cast,
                f"{tree} typed restored-object call in {path.relative_to(ROOT)}",
            )


def validate_game_local_restore_guards(tree: str) -> None:
    source = read(f"src/{tree}/Game_local.cpp")

    for token in (
        "static void GameLocal_ValidateSaveGameCount( idRestoreGame &savegame, int count, int maxCount, const char *detail )",
        "if ( count < 0 || count > maxCount )",
        "GameLocal_ValidateSaveGameCount( savegame, numClients, MAX_CLIENTS, \"client count\" );",
        "GameLocal_ValidateSaveGameCount( savegame, num_entities, MAX_GENTITIES, \"entity count\" );",
        "GameLocal_ValidateSaveGameCount( savegame, num, MAX_GENTITIES, \"spawned entity count\" );",
        "GameLocal_ValidateSaveGameCount( savegame, num, MAX_CENTITIES, \"client spawned entity count\" );",
        "GameLocal_ValidateSaveGameCount( savegame, num, MAX_GENTITIES, \"active entity count\" );",
        "GameLocal_ValidateSaveGameCount( savegame, numEntitiesToDeactivate, MAX_GENTITIES, \"deactivation count\" );",
        "if ( firstFreeIndex < 0 || firstFreeIndex >= MAX_GENTITIES )",
        "invalid first free entity index",
    ):
        require(source, token, f"{tree} game-local savegame restore guard")

    require_regex(
        source,
        r"savegame\.ReadInt\s*\(\s*numClients\s*\)\s*;\s*"
        r"GameLocal_ValidateSaveGameCount\s*\(\s*savegame,\s*numClients,\s*MAX_CLIENTS,\s*\"client count\"\s*\)\s*;\s*"
        r"for\s*\(\s*i\s*=\s*0\s*;\s*i\s*<\s*numClients\s*;\s*i\+\+\s*\)\s*\{.*?"
        r"savegame\.ReadDict\s*\(\s*&persistentPlayerInfo\s*\[\s*i\s*\]\s*\)\s*;",
        f"{tree} persistent player dictionaries are gated by the restored client count",
    )

    require_regex(
        source,
        r"lastAIAlertActor\.Save\s*\(\s*&savegame\s*\)\s*;\s*"
        r"lastAIAlertEntity\.Save\s*\(\s*&savegame\s*\)\s*;\s*"
        r"savegame\.WriteInt\s*\(\s*lastAIAlertEntityTime\s*\)\s*;\s*"
        r"savegame\.WriteInt\s*\(\s*lastAIAlertActorTime\s*\)\s*;",
        f"{tree} AI alert save order",
    )
    require_regex(
        source,
        r"lastAIAlertActor\.Restore\s*\(\s*&savegame\s*\)\s*;\s*"
        r"lastAIAlertEntity\.Restore\s*\(\s*&savegame\s*\)\s*;\s*"
        r"savegame\.ReadInt\s*\(\s*lastAIAlertEntityTime\s*\)\s*;\s*"
        r"savegame\.ReadInt\s*\(\s*lastAIAlertActorTime\s*\)\s*;",
        f"{tree} AI alert restore order",
    )


def validate_entity_restore_guards(tree: str) -> None:
    source = read(f"src/{tree}/Entity.cpp")

    require_regex(
        source,
        r"savefile->ReadInt\s*\(\s*entityNumber\s*\)\s*;\s*"
        r"if\s*\(\s*entityNumber\s*<\s*0\s*\|\|\s*entityNumber\s*>=\s*MAX_GENTITIES\s*\)",
        f"{tree} entity number restore bounds",
    )
    for token in (
        "idEntity::Restore: invalid entity number",
        "idEntity::Restore: invalid target count",
        "if ( num < 0 || num > MAX_SAVEGAME_CLIENT_ENTITY_REFS )",
        "idEntity::Restore: invalid bound client entity count",
        "idEntity::Restore: invalid signal count",
    ):
        require(source, token, f"{tree} entity restore corruption error")

    if source.count("MAX_SAVEGAME_ENTITY_REFS") < 2:
        raise AssertionError(f"{tree} entity restore should cap both targets and signals with MAX_SAVEGAME_ENTITY_REFS")

    reject_regex(
        source,
        r"if\s*\(\s*num\s*<\s*0\s*\|\|\s*num\s*>\s*MAX_CENTITIES\s*\).*?"
        r"idEntity::Restore: invalid bound client entity count",
        f"{tree} bound client entity restore count cap",
    )


def validate_brittle_fracture_guards(tree: str) -> None:
    source = read(f"src/{tree}/BrittleFracture.cpp")

    for token in (
        "if ( num < 0 || num > MAX_SAVEGAME_BRITTLE_SHARDS )",
        "invalid shard count",
        "const int maxShardEdges = shards[i]->winding.GetNumPoints();",
        "if ( maxShardEdges < 0 || maxShardEdges > MAX_SAVEGAME_BRITTLE_EDGES_PER_SHARD )",
        "invalid winding point count",
        "if ( j < 0 || j > MAX_SAVEGAME_BRITTLE_DECALS_PER_SHARD )",
        "invalid decal count",
        "if ( j < 0 || j > maxShardEdges )",
        "invalid neighbour count",
        "if ( j != maxShardEdges )",
        "invalid edge-neighbour count",
    ):
        require(source, token, f"{tree} brittle-fracture savegame corruption guard")


def validate_ai_manager_restore_guards(tree: str) -> None:
    source = read(f"src/{tree}/ai/AI_Manager.cpp")

    for token in (
        "static const int MAX_SAVEGAME_AI_TEAM_MEMBERS = 4096;",
        "static const int MAX_SAVEGAME_AI_AVOIDS = 4096;",
        "teams[i].Clear ( );",
    ):
        require(source, token, f"{tree} AI manager savegame collection guard")

    require_regex(
        source,
        r"const\s+int\s+memberCount\s*=\s*teams\[i\]\.Num\s*\(\s*\)\s*;\s*"
        r"if\s*\(\s*memberCount\s*>\s*MAX_SAVEGAME_AI_TEAM_MEMBERS\s*\)\s*\{\s*"
        r"gameLocal\.Error\s*\(\s*\"rvAIManager::Save: invalid member count %d for team %d\"[^;]*;\s*\}\s*"
        r"savefile->WriteInt\s*\(\s*memberCount\s*\)",
        f"{tree} AI team member count is validated before serialization",
    )
    require_regex(
        source,
        r"if\s*\(\s*avoids\.Num\s*\(\s*\)\s*>\s*MAX_SAVEGAME_AI_AVOIDS\s*\)\s*\{\s*"
        r"gameLocal\.Error\s*\(\s*\"rvAIManager::Save: invalid avoid count %d\"[^;]*;\s*\}\s*"
        r"savefile->WriteInt\s*\(\s*avoids\.Num\s*\(\s*\)\s*\)",
        f"{tree} AI avoid count is validated before serialization",
    )
    require_regex(
        source,
        r"savefile->ReadInt\s*\(\s*j\s*\)\s*;\s*"
        r"if\s*\(\s*j\s*<\s*0\s*\|\|\s*j\s*>\s*MAX_SAVEGAME_AI_TEAM_MEMBERS\s*\)\s*\{\s*"
        r"savefile->Error\s*\(\s*\"rvAIManager::Restore: invalid member count %d for team %d\"[^;]*;\s*\}\s*"
        r"for\s*\(\s*;\s*j\s*>\s*0\s*;\s*j\s*--\s*\)",
        f"{tree} AI team member count is validated before iteration",
    )
    require_regex(
        source,
        r"savefile->ReadInt\s*\(\s*j\s*\)\s*;\s*"
        r"if\s*\(\s*j\s*<\s*0\s*\|\|\s*j\s*>\s*MAX_SAVEGAME_AI_AVOIDS\s*\)\s*\{\s*"
        r"savefile->Error\s*\(\s*\"rvAIManager::Restore: invalid avoid count %d\"[^;]*;\s*\}\s*"
        r"avoids\.SetNum\s*\(\s*j\s*\)",
        f"{tree} AI avoid count is validated before allocation",
    )


def validate_state_restore_guards(tree: str) -> None:
    source = read(f"src/{tree}/gamesys/State.cpp")

    require_regex(
        source,
        r"saveFile->ReadString\s*\(\s*name\s*\)\s*;\s*"
        r"state\s*=\s*owner->FindState\s*\(\s*name\s*\)\s*;\s*"
        r"if\s*\(\s*state\s*==\s*NULL\s*\)\s*\{\s*"
        r"saveFile->Error\s*\(\s*\"stateCall_t::Restore: unknown state '%s' for owner class '%s'\"\s*,\s*"
        r"name\.c_str\s*\(\s*\)\s*,\s*owner->GetClassname\s*\(\s*\)\s*\)\s*;\s*\}\s*"
        r"saveFile->ReadInt\s*\(\s*flags\s*\)",
        f"{tree} restored state names are validated before state payload use",
    )


def validate_representative_count_guards(tree: str) -> None:
    required_sources = {
        f"src/{tree}/Mover.cpp": (
            "if ( buddies.Num() > MAX_SAVEGAME_MOVER_BUDDIES )",
            "idMover_Binary::Save: invalid buddy count",
            "buddies.Clear();",
            "if ( num < 0 || num > MAX_SAVEGAME_MOVER_BUDDIES )",
            "idMover_Binary::Restore: invalid buddy count",
        ),
        f"src/{tree}/Player.cpp": (
            "if ( num < 0 || num > MAX_SAVEGAME_INVENTORY_ITEMS )",
            "invalid item count",
            "if ( num < 0 || num > MAX_SAVEGAME_LEVEL_TRIGGERS )",
            "invalid level trigger count",
            "if ( num < 0 || num > MAX_SAVEGAME_PICKUP_ITEMS )",
            "invalid pickup item count",
            "if ( num < 0 || num > MAX_SAVEGAME_OBJECTIVES )",
            "invalid objective count",
            "if ( num < 0 || num > MAX_SAVEGAME_AAS_LOCATIONS )",
            "invalid AAS location count",
        ),
        f"src/{tree}/physics/Physics_Base.cpp": (
            "if ( num < 0 || num > MAX_SAVEGAME_PHYSICS_CONTACTS )",
            "invalid contact count",
            "if ( num < 0 || num > MAX_SAVEGAME_PHYSICS_CONTACT_ENTITIES )",
            "invalid contact entity count",
        ),
        f"src/{tree}/gamesys/State.cpp": (
            "if ( numStates < 0 || numStates > MAX_SAVEGAME_STATE_CALLS )",
            "invalid active state count",
            "invalid interrupted state count",
        ),
        f"src/{tree}/script/ScriptFuncUtility.cpp": (
            "if ( numParms < 0 || numParms > MAX_SAVEGAME_SCRIPT_FUNC_PARMS )",
            "invalid parameter count",
        ),
    }
    for relative_path, tokens in required_sources.items():
        source = read(relative_path)
        for token in tokens:
            require(source, token, f"{tree} representative restore count guard in {relative_path}")


def reject_nested_count_regressions(tree: str) -> None:
    nested_restore_sources = (
        f"src/{tree}/BrittleFracture.cpp",
        f"src/{tree}/Player.cpp",
        f"src/{tree}/physics/Physics_Base.cpp",
        f"src/{tree}/gamesys/State.cpp",
        f"src/{tree}/script/ScriptFuncUtility.cpp",
    )
    broad_count_pattern = (
        r"if\s*\(\s*(?P<var>num|j|numStates|numParms)\s*<\s*0\s*\|\|\s*"
        r"(?P=var)\s*>\s*MAX_GENTITIES\s*\)"
    )
    for relative_path in nested_restore_sources:
        reject_regex(read(relative_path), broad_count_pattern, f"{tree} nested restore count caps in {relative_path}")


def validate_sp_mp_restore_guard_parity() -> None:
    sp_guards = extract_restore_guard_signatures("game")
    mp_guards = extract_restore_guard_signatures("mpgame")

    sp_only = sorted(sp_guards - mp_guards)
    mp_only = sorted(mp_guards - sp_guards)
    if sp_only or mp_only:
        details: list[str] = []
        if sp_only:
            details.append("SP-only restore guards:")
            details.extend(f"  {path}: if ({condition}) -> {message}" for path, condition, message in sp_only)
        if mp_only:
            details.append("MP-only restore guards:")
            details.extend(f"  {path}: if ({condition}) -> {message}" for path, condition, message in mp_only)
        raise AssertionError("SP/MP restore guard parity drifted:\n" + "\n".join(details))


def validate_portable_layout_migrations(tree: str) -> None:
    savegame_header = read(f"src/{tree}/gamesys/SaveGame.h")
    savegame_source = read(f"src/{tree}/gamesys/SaveGame.cpp")
    accessor = "GetOpenQ4SaveGameCompatibilityVersion( void ) const"
    require(savegame_header, accessor, f"{tree} savegame version accessor declaration")
    require(savegame_source, accessor, f"{tree} savegame version accessor definition")

    version_branch = (
        "GetOpenQ4SaveGameCompatibilityVersion() == "
        "OPENQ4_SAVEGAME_COMPATIBILITY_VERSION"
    )
    migrations = {
        "Entity.cpp": (
            "packedFlags |= fl.quickBurn ? BIT( 19 ) : 0;",
            "savefile->WriteInt( packedFlags );",
            "savefile->ReadInt( packedFlags );",
            "savefile->Read( &fl, sizeof( fl ) );",
        ),
        "Player.cpp": (
            "packedFlags |= pfl.noFallingDamage ? BIT( 21 ) : 0;",
            "savefile->WriteInt( packedFlags );",
            "savefile->ReadInt( packedFlags );",
            "savefile->Read( &pfl, sizeof( pfl ) );",
        ),
        "Weapon.cpp": (
            "packedWeaponFlags |= wfl.hasWindupAnim ? BIT( 8 ) : 0;",
            "savefile->WriteInt( packedStateFlags );",
            "savefile->ReadInt( packedWeaponFlags );",
            "savefile->Read( &wsfl, sizeof( wsfl ) );",
            "savefile->Read( &wfl, sizeof( wfl ) );",
        ),
        "Projectile.cpp": (
            "packedFlags |= projectileFlags.isTracer ? BIT( 4 ) : 0;",
            "savefile->WriteInt( packedFlags );",
            "savefile->ReadInt( packedFlags );",
            "savefile->Read( &projectileFlags, sizeof( projectileFlags ) );",
        ),
        "Mover.cpp": (
            "savefile->WriteVec3( move.dir );",
            "savefile->WriteAngles( rot.rot );",
            "idMover::Restore: invalid movement stage",
            "idMover::Restore: invalid rotation stage",
            "savefile->Read( &move, sizeof( move ) );",
            "savefile->Read( &rot, sizeof( rot ) );",
        ),
        "IK.cpp": (
            "savefile->WriteJoint( footJoints[i] );",
            "savefile->WriteMat3( upperLegToHipJoint[i] );",
            "savefile->Read( footJoints, sizeof( footJoints ) );",
            "savefile->Read( upperLegToHipJoint, sizeof( upperLegToHipJoint ) );",
            "idIK_Walk::Restore: invalid leg count",
            "idIK_Reach::Restore: invalid arm count",
        ),
        "gamesys/State.cpp": (
            "packedFlags |= fl.executing ? BIT( 2 ) : 0;",
            "saveFile->WriteInt( packedFlags );",
            "saveFile->ReadInt( packedFlags );",
            "saveFile->Read( &fl, sizeof( fl ) );",
        ),
        "physics/Physics_RigidBody.cpp": (
            "savefile->WriteVec6( state.pushVelocity );",
            "savefile->ReadVec6( state.pushVelocity );",
            "savefile->Read( &state.pushVelocity, sizeof( state.pushVelocity ) );",
        ),
        "ai/AI_Actions.cpp": (
            "packedFlags |= fl.noSimpleThink ? BIT( 6 ) : 0;",
            "savefile->Read( &fl, sizeof( fl ) );",
        ),
        "ai/AI_Move.cpp": (
            "packedFlags |= fl.noRangedInterrupt ? BIT( 22 ) : 0;",
            "savefile->Read( &fl, sizeof( fl ) );",
        ),
        "ai/AI.cpp": (
            "packedFlags |= aifl.killerGuard ? BIT( 20 ) : 0;",
            "packedFlags |= combat.fl.crouchViewClear ? BIT( 8 ) : 0;",
            "packedFlags |= passive.fl.fidget ? BIT( 2 ) : 0;",
            "packedFlags |= enemy.fl.visible ? BIT( 4 ) : 0;",
            "savefile->Read( &aifl, sizeof( aifl ) );",
            "savefile->Read( &combat.fl, sizeof( combat.fl ) );",
            "savefile->Read( &passive.fl, sizeof( passive.fl ) );",
            "savefile->Read( &enemy.fl, sizeof( enemy.fl ) );",
        ),
    }

    for relative_path, tokens in migrations.items():
        source = read(f"src/{tree}/{relative_path}")
        require(source, version_branch, f"{tree} v2/v3 layout branch in {relative_path}")
        for token in tokens:
            require(source, token, f"{tree} portable layout migration in {relative_path}")

    packed_layouts = (
        (
            "Entity.cpp",
            "fl",
            "packedFlags",
            (
                "notarget", "noknockback", "takedamage", "hidden", "bindOrientated",
                "solidForTeam", "forcePhysicsUpdate", "selected", "neverDormant", "isDormant",
                "hasAwakened", "networkSync", "networkStale", "triggerAnim", "usable",
                "isAIObstacle", "persistAcrossInstances", "exitedVehicle", "allowAutoBlink", "quickBurn",
            ),
        ),
        (
            "Player.cpp",
            "pfl",
            "packedFlags",
            (
                "forward", "backward", "strafeLeft", "strafeRight", "attackHeld", "weaponFired",
                "jump", "crouch", "onGround", "onLadder", "dead", "run", "pain", "hardLanding",
                "softLanding", "reload", "teleport", "turnLeft", "turnRight", "hearingLoss",
                "objectiveFailed", "noFallingDamage",
            ),
        ),
        (
            "Projectile.cpp",
            "projectileFlags",
            "packedFlags",
            (
                "detonate_on_world", "detonate_on_actor", "detonate_on_bounce",
                "randomShaderSpin", "isTracer",
            ),
        ),
        (
            "Weapon.cpp",
            "wsfl",
            "packedStateFlags",
            (
                "attack", "reload", "netReload", "netEndReload", "raiseWeapon", "lowerWeapon",
                "flashlight", "zoom",
            ),
        ),
        (
            "Weapon.cpp",
            "wfl",
            "packedWeaponFlags",
            (
                "attackAltHitscan", "attackHitscan", "hide", "disabled", "hasBloodSplat",
                "silent_fire", "zoomHideCrosshair", "flashlightOn", "hasWindupAnim",
            ),
        ),
        (
            "ai/AI_Actions.cpp",
            "fl",
            "packedFlags",
            ("disabled", "noPain", "noTurn", "isAttack", "isMelee", "overrideLegs", "noSimpleThink"),
        ),
        (
            "ai/AI_Move.cpp",
            "fl",
            "packedFlags",
            (
                "done", "moving", "crouching", "running", "blocked", "obstacleInPath",
                "goalUnreachable", "onGround", "flyTurning", "idealRunning", "disabled",
                "ignoreObstacles", "allowDirectional", "allowAnimMove", "allowPrevAnimMove",
                "allowHiddenMove", "allowPushMovables", "allowSlideToGoal", "noRun", "noWalk",
                "noTurn", "noGravity", "noRangedInterrupt",
            ),
        ),
        (
            "ai/AI.cpp",
            "aifl",
            "packedFlags",
            (
                "awake", "damage", "pain", "dead", "activated", "jump", "hitEnemy", "pushed",
                "disableAttacks", "scriptedEndWithIdle", "scriptedNeverDormant", "scripted",
                "simpleThink", "ignoreFlashlight", "action", "lookAtPlayer", "disableLook",
                "undying", "tetherMover", "meleeSuperhero", "killerGuard",
            ),
        ),
        (
            "ai/AI.cpp",
            "combat.fl",
            "packedFlags",
            (
                "ignoreEnemies", "alert", "aware", "tetherNoBreak", "tetherAutoBreak",
                "tetherOutOfRange", "seenEnemyDirectly", "noChatter", "crouchViewClear",
            ),
        ),
        (
            "ai/AI.cpp",
            "passive.fl",
            "packedFlags",
            ("disabled", "multipleIdles", "fidget"),
        ),
        (
            "ai/AI.cpp",
            "enemy.fl",
            "packedFlags",
            ("lockOrigin", "dead", "inFov", "sighted", "visible"),
        ),
        (
            "gamesys/State.cpp",
            "fl",
            "packedFlags",
            ("stateCleared", "stateInterrupted", "executing"),
        ),
    )
    for relative_path, prefix, packed_name, fields in packed_layouts:
        source = read(f"src/{tree}/{relative_path}")
        for bit, field in enumerate(fields):
            require(
                source,
                f"{packed_name} |= {prefix}.{field} ? BIT( {bit} ) : 0;",
                f"{tree} packed write schema for {prefix}.{field}",
            )
            require(
                source,
                f"{prefix}.{field} = ( {packed_name} & BIT( {bit} ) ) != 0;",
                f"{tree} packed restore schema for {prefix}.{field}",
            )

    actual_raw_writes: dict[str, int] = {}
    source_root = ROOT / "src" / tree
    for path in sorted(source_root.rglob("*.cpp")):
        count = len(RAW_SAVE_WRITE_PATTERN.findall(path.read_text(encoding="utf-8")))
        if count:
            relative_path = str(path.relative_to(source_root)).replace("\\", "/")
            actual_raw_writes[relative_path] = count
    if actual_raw_writes != EXPECTED_RAW_SAVE_WRITE_COUNTS:
        raise AssertionError(
            f"{tree} raw save-write inventory drifted:\n"
            f"expected={EXPECTED_RAW_SAVE_WRITE_COUNTS}\nactual={actual_raw_writes}"
        )


def validate_packed_flag_wire_model() -> None:
    for width in (3, 5, 7, 8, 9, 20, 21, 22, 23):
        mask = (1 << width) - 1
        samples = (0, mask, mask & 0x55555555, mask & 0xAAAAAAAA)
        for sample in samples:
            fields = tuple(bool(sample & (1 << bit)) for bit in range(width))
            packed = sum((1 << bit) for bit, enabled in enumerate(fields) if enabled)
            if packed != sample:
                raise AssertionError(
                    f"packed flag model failed for width={width}: {packed:#x} != {sample:#x}"
                )
            restored = tuple(bool(packed & (1 << bit)) for bit in range(width))
            if restored != fields:
                raise AssertionError(f"packed flag restore model failed for width={width}")
            if struct.unpack("<i", struct.pack("<i", packed))[0] != packed:
                raise AssertionError(f"packed flag fixed-width encoding failed for width={width}")


def validate_ci_wiring() -> None:
    workflow = read(".github/workflows/commit-validation.yml")
    for token in (
        "tools/tests/savegame_corruption_contract.py",
        "python tools/tests/savegame_corruption_contract.py",
    ):
        require(workflow, token, "GameLibs savegame corruption contract CI wiring")


def main() -> None:
    validate_restore_fuzz_model()
    validate_packed_flag_wire_model()
    for tree in GAME_TREES:
        validate_domain_caps(tree)
        validate_restore_io_guards(tree)
        validate_object_reference_guards(tree)
        validate_game_local_restore_guards(tree)
        validate_entity_restore_guards(tree)
        validate_brittle_fracture_guards(tree)
        validate_ai_manager_restore_guards(tree)
        validate_state_restore_guards(tree)
        validate_representative_count_guards(tree)
        reject_nested_count_regressions(tree)
        validate_portable_layout_migrations(tree)
    validate_sp_mp_restore_guard_parity()
    validate_ci_wiring()
    print("savegame_corruption_contract: ok")


if __name__ == "__main__":
    main()
