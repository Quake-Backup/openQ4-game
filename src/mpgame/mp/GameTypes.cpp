//----------------------------------------------------------------
// GameTypes.cpp
//
// openQ4 multiplayer gametype descriptor table.
//----------------------------------------------------------------

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "GameTypes.h"

/*
===============================================================================

	The gametype table.

	Indexed by gameType_t.  gameType_t is the first byte of every gamestate
	packet and is compared literally by the .gui files, so it is append-only:
	never insert or renumber, only add before NUM_GAME_TYPES.

	entityFilter selects which map entities spawn.  New modes borrow the
	entity layout of the Quake 4 mode they are shaped like, so they play on
	stock maps without new map content: the round modes borrow Team DM,
	the obelisk and turn modes borrow CTF, and Domination borrows Arena CTF
	so the assault point chain spawns as its control points.

	mapDeclKey is the fallback key checked in the map decl when a map does
	not explicitly advertise the mode.

===============================================================================
*/

static const mpGameTypeInfo_t mpGameTypeInfoTable[] = {
	{	GAME_SP,				"singleplayer",			"sp",		"#str_107679",	"",					"singleplayer",
		GTF_SINGLEPLAYER },

	{	GAME_DM,				"DM",					"dm",		"#str_107679",	"DM",				"DM",
		GTF_FRAGLIMIT | GTF_BUYING },

	{	GAME_TOURNEY,			"Tourney",				"tourney",	"#str_107676",	"Tourney",			"Tourney",
		GTF_FRAGLIMIT | GTF_BRACKET },

	{	GAME_TDM,				"Team DM",				"tdm",		"#str_107677",	"Team DM",			"Team DM",
		GTF_TEAM | GTF_FRAGLIMIT | GTF_BUYING },

	{	GAME_CTF,				"CTF",					"ctf",		"#str_107678",	"CTF",				"CTF",
		GTF_TEAM | GTF_FLAG | GTF_CAPTURELIMIT },

	{	GAME_1F_CTF,			"One Flag CTF",			"1fctf",	"#str_107680",	"CTF",				"CTF",
		GTF_TEAM | GTF_FLAG | GTF_ONEFLAG | GTF_CAPTURELIMIT },

	{	GAME_ARENA_CTF,			"Arena CTF",			"actf",		"#str_107681",	"Arena CTF",		"Arena CTF",
		GTF_TEAM | GTF_FLAG | GTF_ARENA | GTF_CAPTURELIMIT },

	{	GAME_ARENA_1F_CTF,		"Arena One Flag CTF",	"a1fctf",	"#str_107682",	"Arena CTF",		"Arena CTF",
		GTF_TEAM | GTF_FLAG | GTF_ONEFLAG | GTF_ARENA | GTF_CAPTURELIMIT },

	{	GAME_DEADZONE,			"DeadZone",				"dz",		"#str_122001",	"DeadZone",			"DeadZone",
		GTF_TEAM | GTF_DEADZONE | GTF_BUYING },

	// openQ4: gametypes carried over from Quake Live
	{	GAME_DUEL,				"Duel",					"duel",		"#str_41300",	"DM",				"DM",
		GTF_DUEL | GTF_FRAGLIMIT },

	{	GAME_CA,				"Clan Arena",			"ca",		"#str_41301",	"Team DM",			"Team DM",
		GTF_TEAM | GTF_ROUND | GTF_ELIMINATION | GTF_ROUNDLIMIT | GTF_DAMAGESCORE },

	{	GAME_FREEZETAG,			"Freeze Tag",			"ft",		"#str_41302",	"Team DM",			"Team DM",
		GTF_TEAM | GTF_ROUND | GTF_ELIMINATION | GTF_FREEZE | GTF_ROUNDLIMIT },

	{	GAME_REDROVER,			"Red Rover",			"rr",		"#str_41303",	"Team DM",			"Team DM",
		GTF_TEAM | GTF_ROUND | GTF_TEAMSWAP | GTF_ROUNDLIMIT },

	{	GAME_OVERLOAD,			"Overload",				"ovl",		"#str_41304",	"CTF",				"CTF",
		GTF_TEAM | GTF_OBELISK | GTF_CAPTURELIMIT },

	{	GAME_HARVESTER,			"Harvester",			"har",		"#str_41305",	"CTF",				"CTF",
		GTF_TEAM | GTF_OBELISK | GTF_CAPTURELIMIT },

	{	GAME_DOMINATION,		"Domination",			"dom",		"#str_41306",	"Arena CTF",		"Arena CTF",
		GTF_TEAM | GTF_CONTROLPOINT | GTF_SCORELIMIT },

	{	GAME_ATTACK_DEFEND,		"Attack Defend",		"ad",		"#str_41307",	"CTF",				"CTF",
		GTF_TEAM | GTF_FLAG | GTF_ONEFLAG | GTF_ROUND | GTF_ELIMINATION | GTF_TURNS | GTF_SCORELIMIT | GTF_DAMAGESCORE },
};

const int mpNumGameTypeInfo = sizeof( mpGameTypeInfoTable ) / sizeof( mpGameTypeInfoTable[ 0 ] );

/*
================
si_gameTypeArgs

Mirrors the name column of the table above; kept adjacent so the two cannot
drift unnoticed, and cross-checked at startup by MPValidateGameTypeTable.
Both arrays are constant-initialised, so si_gameType's constructor may read
this safely regardless of translation unit initialisation order.
================
*/
const char *si_gameTypeArgs[] = {
	"singleplayer",
	"DM",
	"Tourney",
	"Team DM",
	"CTF",
	"One Flag CTF",
	"Arena CTF",
	"Arena One Flag CTF",
	"DeadZone",
	"Duel",
	"Clan Arena",
	"Freeze Tag",
	"Red Rover",
	"Overload",
	"Harvester",
	"Domination",
	"Attack Defend",
	NULL
};

const int si_numGameTypeArgs = sizeof( si_gameTypeArgs ) / sizeof( si_gameTypeArgs[ 0 ] );

/*
================
MPGameType
================
*/
const mpGameTypeInfo_t *MPGameType( int type ) {
	if ( type < 0 || type >= mpNumGameTypeInfo ) {
		return &mpGameTypeInfoTable[ GAME_DM ];
	}

	return &mpGameTypeInfoTable[ type ];
}

/*
================
MPGameTypeByName
================
*/
const mpGameTypeInfo_t *MPGameTypeByName( const char *name ) {
	int i;

	if ( name == NULL || *name == '\0' ) {
		return NULL;
	}

	for ( i = 0; i < mpNumGameTypeInfo; i++ ) {
		if ( idStr::Icmp( mpGameTypeInfoTable[ i ].name, name ) == 0 ) {
			return &mpGameTypeInfoTable[ i ];
		}
	}

	return NULL;
}

/*
================
MPGameTypeFlags
================
*/
int MPGameTypeFlags( int type ) {
	return MPGameType( type )->flags;
}

/*
================
MPGameTypeName
================
*/
const char *MPGameTypeName( int type ) {
	return MPGameType( type )->name;
}

/*
================
MPGameTypeLocalizedName
================
*/
const char *MPGameTypeLocalizedName( int type ) {
	return common->GetLocalizedString( MPGameType( type )->localizedName );
}

/*
================
MPGameTypeHasAll
================
*/
bool MPGameTypeHasAll( int type, int mask ) {
	return ( ( MPGameType( type )->flags & mask ) == mask );
}

/*
================
MPGameTypeHasAny
================
*/
bool MPGameTypeHasAny( int type, int mask ) {
	return ( ( MPGameType( type )->flags & mask ) != 0 );
}

/*
================
MPMapSupportsGameType

A map advertises support with a boolean key named after the gametype.  Stock
Quake 4 maps only know about the stock modes, so a mode that borrows another
mode's entity layout is also playable wherever that base mode is.
================
*/
bool MPMapSupportsGameType( const idDict *mapDict, int type ) {
	const mpGameTypeInfo_t *info;

	if ( mapDict == NULL ) {
		return false;
	}

	info = MPGameType( type );

	if ( mapDict->GetBool( info->name ) ) {
		return true;
	}

	if ( info->mapDeclKey != NULL && idStr::Icmp( info->mapDeclKey, info->name ) != 0 ) {
		return mapDict->GetBool( info->mapDeclKey );
	}

	return false;
}

/*
================
MPMapSupportsGameTypeName
================
*/
bool MPMapSupportsGameTypeName( const idDict *mapDict, const char *name ) {
	const mpGameTypeInfo_t *info;

	if ( mapDict == NULL ) {
		return false;
	}

	info = MPGameTypeByName( name );
	if ( info == NULL ) {
		// unknown token; fall back to the literal key so map packs that
		// advertise their own modes keep working
		return mapDict->GetBool( name );
	}

	return MPMapSupportsGameType( mapDict, info->type );
}

/*
================
mpVoteGameTypeOrder

Order the gametype dropdowns and the vote packet use.  The first six entries
must stay where they are: they are the shipped vote_gametype_t values and the
stock .gui dropdowns index them positionally.
================
*/
static const int mpVoteGameTypeOrder[] = {
	GAME_DM,
	GAME_TOURNEY,
	GAME_TDM,
	GAME_CTF,
	GAME_ARENA_CTF,
	GAME_DEADZONE,
	// openQ4 additions
	GAME_DUEL,
	GAME_CA,
	GAME_FREEZETAG,
	GAME_REDROVER,
	GAME_1F_CTF,
	GAME_ARENA_1F_CTF,
	GAME_OVERLOAD,
	GAME_HARVESTER,
	GAME_DOMINATION,
	GAME_ATTACK_DEFEND
};

static const int mpNumVoteGameTypes = sizeof( mpVoteGameTypeOrder ) / sizeof( mpVoteGameTypeOrder[ 0 ] );

/*
================
MPVoteGameTypeCount
================
*/
int MPVoteGameTypeCount( void ) {
	return mpNumVoteGameTypes;
}

/*
================
MPVoteGameTypeToGameType
================
*/
int MPVoteGameTypeToGameType( int voteIndex ) {
	if ( voteIndex < 0 || voteIndex >= mpNumVoteGameTypes ) {
		return GAME_DM;
	}

	return mpVoteGameTypeOrder[ voteIndex ];
}

/*
================
MPGameTypeToVoteGameType
================
*/
int MPGameTypeToVoteGameType( int type ) {
	int i;

	for ( i = 0; i < mpNumVoteGameTypes; i++ ) {
		if ( mpVoteGameTypeOrder[ i ] == type ) {
			return i;
		}
	}

	return 0;	// VOTE_GAMETYPE_DM
}

/*
================
MPValidateGameTypeTable

Catches the two ways this table can silently rot: a gametype added to the
enum without a table row, and a name column that has drifted from the
si_gameType completion list.
================
*/
void MPValidateGameTypeTable( void ) {
	int i;

	if ( mpNumGameTypeInfo != NUM_GAME_TYPES ) {
		gameLocal.Error( "MPValidateGameTypeTable: gametype table has %d rows, gameType_t has %d values", mpNumGameTypeInfo, NUM_GAME_TYPES );
		return;
	}

	if ( si_numGameTypeArgs != NUM_GAME_TYPES + 1 ) {
		gameLocal.Error( "MPValidateGameTypeTable: si_gameTypeArgs has %d entries, expected %d", si_numGameTypeArgs, NUM_GAME_TYPES + 1 );
		return;
	}

	for ( i = 0; i < mpNumGameTypeInfo; i++ ) {
		if ( mpGameTypeInfoTable[ i ].type != i ) {
			gameLocal.Error( "MPValidateGameTypeTable: row %d declares gametype %d; the table must be indexed by gameType_t", i, mpGameTypeInfoTable[ i ].type );
			return;
		}

		if ( idStr::Cmp( mpGameTypeInfoTable[ i ].name, si_gameTypeArgs[ i ] ) != 0 ) {
			gameLocal.Error( "MPValidateGameTypeTable: row %d is '%s' but si_gameTypeArgs[%d] is '%s'", i, mpGameTypeInfoTable[ i ].name, i, si_gameTypeArgs[ i ] );
			return;
		}

		// every playable mode must be reachable from the vote and menu lists
		if ( ( mpGameTypeInfoTable[ i ].flags & GTF_SINGLEPLAYER ) == 0 ) {
			if ( MPVoteGameTypeToGameType( MPGameTypeToVoteGameType( i ) ) != i ) {
				gameLocal.Error( "MPValidateGameTypeTable: gametype '%s' is missing from mpVoteGameTypeOrder", mpGameTypeInfoTable[ i ].name );
				return;
			}
		}
	}
}
