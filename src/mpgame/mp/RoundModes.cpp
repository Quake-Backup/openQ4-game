//----------------------------------------------------------------
// RoundModes.cpp
//
// openQ4 round based gametypes carried over from Quake Live.
//----------------------------------------------------------------

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "RoundModes.h"

gameStateType_t rvClanArenaGameState::type = GS_CA;
gameStateType_t rvFreezeTagGameState::type = GS_FREEZETAG;
gameStateType_t rvRedRoverGameState::type = GS_REDROVER;

// Clan Arena pays a point of personal score per this much damage dealt
const int CA_DAMAGE_PER_SCORE = 100;

/*
===============================================================================

rvClanArenaGameState

===============================================================================
*/

/*
================
rvClanArenaGameState::rvClanArenaGameState
================
*/
rvClanArenaGameState::rvClanArenaGameState( bool allocPrevious ) : rvRoundGameState( false ) {
	Clear();

	if ( allocPrevious ) {
		previousGameState = new rvClanArenaGameState( false );
	} else {
		previousGameState = NULL;
	}

	trackPrevious = allocPrevious;
}

/*
================
rvClanArenaGameState::Clear
================
*/
void rvClanArenaGameState::Clear( void ) {
	int i;

	rvRoundGameState::Clear();

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		damageResidue[ i ] = 0;
	}
}

/*
================
rvClanArenaGameState::RoundBegin

Everyone starts each round on equal terms: full arsenal, full ammo, no
scavenging.  That is what makes Clan Arena a pure fight rather than a map
control game.
================
*/
void rvClanArenaGameState::RoundBegin( void ) {
	int i;

	rvRoundGameState::RoundBegin();

	if ( gameLocal.isClient ) {
		return;
	}

	for ( i = 0; i < gameLocal.numClients; i++ ) {
		idEntity *ent = gameLocal.entities[ i ];

		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}

		idPlayer *p = static_cast< idPlayer * >( ent );
		if ( p->spectating || p->wantSpectate ) {
			continue;
		}

		GiveStuffToPlayer( p, "weapons", "" );
		GiveStuffToPlayer( p, "ammo", "" );
		GiveStuffToPlayer( p, "armor", "" );
		GiveStuffToPlayer( p, "health", "" );

		damageResidue[ i ] = 0;
	}
}

/*
================
rvClanArenaGameState::PlayerDamage
================
*/
void rvClanArenaGameState::PlayerDamage( idPlayer* attacker, idPlayer* victim, int damage ) {
	int index, award;

	if ( attacker == NULL || victim == NULL || attacker == victim ) {
		return;
	}

	if ( !RoundIsLive() || damage <= 0 ) {
		return;
	}

	// no credit for shooting your own side
	if ( attacker->team == victim->team ) {
		return;
	}

	index = attacker->entityNumber;
	if ( index < 0 || index >= MAX_CLIENTS ) {
		return;
	}

	damageResidue[ index ] += damage;

	award = damageResidue[ index ] / CA_DAMAGE_PER_SCORE;
	if ( award > 0 ) {
		damageResidue[ index ] -= award * CA_DAMAGE_PER_SCORE;
		gameLocal.mpGame.AddPlayerScore( attacker, award );
	}
}

/*
================
rvClanArenaGameState::IsType
================
*/
bool rvClanArenaGameState::IsType( gameStateType_t t ) const {
	return ( t == rvClanArenaGameState::type );
}

/*
================
rvClanArenaGameState::GetClassType
================
*/
gameStateType_t rvClanArenaGameState::GetClassType( void ) {
	return rvClanArenaGameState::type;
}

/*
===============================================================================

rvFreezeTagGameState

===============================================================================
*/

/*
================
rvFreezeTagGameState::rvFreezeTagGameState
================
*/
rvFreezeTagGameState::rvFreezeTagGameState( bool allocPrevious ) : rvRoundGameState( false ) {
	Clear();

	if ( allocPrevious ) {
		previousGameState = new rvFreezeTagGameState( false );
	} else {
		previousGameState = NULL;
	}

	trackPrevious = allocPrevious;
}

/*
================
rvFreezeTagGameState::Clear
================
*/
void rvFreezeTagGameState::Clear( void ) {
	int i;

	rvRoundGameState::Clear();

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		thawProgress[ i ] = 0;
		lastThawAnnounce[ i ] = 0;
	}
}

/*
================
rvFreezeTagGameState::RoundBegin
================
*/
void rvFreezeTagGameState::RoundBegin( void ) {
	int i;

	rvRoundGameState::RoundBegin();

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		thawProgress[ i ] = 0;
		lastThawAnnounce[ i ] = 0;
	}
}

/*
================
rvFreezeTagGameState::PlayerDeath
================
*/
void rvFreezeTagGameState::PlayerDeath( idPlayer* dead, idPlayer* killer ) {
	rvRoundGameState::PlayerDeath( dead, killer );

	if ( dead == NULL || gameLocal.isClient ) {
		return;
	}

	if ( !RoundIsLive() ) {
		return;
	}

	thawProgress[ dead->entityNumber ] = 0;
	lastThawAnnounce[ dead->entityNumber ] = 0;

	gameLocal.mpGame.CenterPrint( dead->entityNumber, "#str_41350" );

	if ( killer != NULL && killer != dead ) {
		gameLocal.mpGame.CenterPrintTeam( dead->team, "#str_41351", idMultiplayerGame::CPARM_CLIENT, killer->entityNumber );
	}
}

/*
================
rvFreezeTagGameState::ClientDisconnect
================
*/
void rvFreezeTagGameState::ClientDisconnect( idPlayer* player ) {
	rvRoundGameState::ClientDisconnect( player );

	if ( player != NULL ) {
		thawProgress[ player->entityNumber ] = 0;
		lastThawAnnounce[ player->entityNumber ] = 0;
	}
}

/*
================
rvFreezeTagGameState::ThawPlayer

Brings a frozen player back into the round where they fell, which is the whole
point of the mode: position matters, so a good thaw is a push.
================
*/
void rvFreezeTagGameState::ThawPlayer( idPlayer* frozen, idPlayer* thawer ) {
	idVec3		origin;
	idAngles	angles;

	if ( frozen == NULL ) {
		return;
	}

	origin = frozen->GetPhysics()->GetOrigin();
	angles = frozen->GetPhysics()->GetAxis().ToAngles();
	angles.pitch = 0.0f;
	angles.roll = 0.0f;

	SetEliminated( frozen->entityNumber, false );
	thawProgress[ frozen->entityNumber ] = 0;

	frozen->forceRespawn = true;
	frozen->ServerSpectate( false );
	frozen->Teleport( origin, angles, NULL );

	if ( thawer != NULL ) {
		gameLocal.mpGame.AddPlayerScore( thawer, 1 );
		gameLocal.mpGame.CenterPrint( frozen->entityNumber, "#str_41352", idMultiplayerGame::CPARM_CLIENT, thawer->entityNumber );
		gameLocal.mpGame.CenterPrintTeam( frozen->team, "#str_41353",
			idMultiplayerGame::CPARM_CLIENT, thawer->entityNumber );
	}
}

/*
================
rvFreezeTagGameState::Run

Ticks the thaw timers.  A frozen player thaws once a live team mate has stood
within si_freezeThawRadius of them for si_freezeThawTime seconds; stepping
away lets the progress drain back rather than resetting it outright, so a
contested thaw is a real contest.
================
*/
void rvFreezeTagGameState::Run( void ) {
	int		i, j;
	int		thawTime, thawRadiusSquared;

	rvRoundGameState::Run();

	if ( gameLocal.isClient || !RoundIsLive() ) {
		return;
	}

	thawTime = Max( 1, gameLocal.serverInfo.GetInt( "si_freezeThawTime" ) ) * 1000;
	thawRadiusSquared = gameLocal.serverInfo.GetInt( "si_freezeThawRadius" );
	thawRadiusSquared *= thawRadiusSquared;

	for ( i = 0; i < gameLocal.numClients; i++ ) {
		idEntity *ent = gameLocal.entities[ i ];

		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}

		idPlayer *frozen = static_cast< idPlayer * >( ent );
		if ( !IsEliminated( i ) || frozen->wantSpectate || frozen->team < 0 ) {
			continue;
		}

		idPlayer *thawer = NULL;
		idVec3 frozenOrigin = frozen->GetPhysics()->GetOrigin();

		for ( j = 0; j < gameLocal.numClients; j++ ) {
			idEntity *other = gameLocal.entities[ j ];

			if ( j == i || !other || !other->IsType( idPlayer::GetClassType() ) ) {
				continue;
			}

			idPlayer *mate = static_cast< idPlayer * >( other );
			if ( mate->team != frozen->team || !PlayerIsAlive( mate ) || mate->spectating ) {
				continue;
			}

			if ( ( mate->GetPhysics()->GetOrigin() - frozenOrigin ).LengthSqr() > thawRadiusSquared ) {
				continue;
			}

			thawer = mate;
			break;
		}

		if ( thawer != NULL ) {
			thawProgress[ i ] += gameLocal.msec;

			// a heartbeat while the thaw runs, so both sides know it is happening
			if ( gameLocal.time - lastThawAnnounce[ i ] > 1000 ) {
				lastThawAnnounce[ i ] = gameLocal.time;
				gameLocal.mpGame.CenterPrint( thawer->entityNumber, "#str_41354" );
			}

			if ( thawProgress[ i ] >= thawTime ) {
				ThawPlayer( frozen, thawer );
			}
		} else if ( thawProgress[ i ] > 0 ) {
			thawProgress[ i ] = Max( 0, thawProgress[ i ] - gameLocal.msec );
		}
	}
}

/*
================
rvFreezeTagGameState::IsType
================
*/
bool rvFreezeTagGameState::IsType( gameStateType_t t ) const {
	return ( t == rvFreezeTagGameState::type );
}

/*
================
rvFreezeTagGameState::GetClassType
================
*/
gameStateType_t rvFreezeTagGameState::GetClassType( void ) {
	return rvFreezeTagGameState::type;
}

/*
===============================================================================

rvRedRoverGameState

===============================================================================
*/

/*
================
rvRedRoverGameState::rvRedRoverGameState
================
*/
rvRedRoverGameState::rvRedRoverGameState( bool allocPrevious ) : rvRoundGameState( false ) {
	Clear();

	if ( allocPrevious ) {
		previousGameState = new rvRedRoverGameState( false );
	} else {
		previousGameState = NULL;
	}

	trackPrevious = allocPrevious;
}

/*
================
rvRedRoverGameState::RoundBegin
================
*/
void rvRedRoverGameState::RoundBegin( void ) {
	int i;

	rvRoundGameState::RoundBegin();

	if ( gameLocal.isClient ) {
		return;
	}

	// a fresh round starts from an even split, or the previous round's winner
	// simply keeps everybody
	for ( i = 0; i < gameLocal.numClients; i++ ) {
		idEntity *ent = gameLocal.entities[ i ];

		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}

		idPlayer *p = static_cast< idPlayer * >( ent );
		if ( p->wantSpectate ) {
			continue;
		}

		GiveStuffToPlayer( p, "ammo", "" );
	}
}

/*
================
rvRedRoverGameState::PlayerDeath

The whole mode in one rule: whoever kills you gains you.
================
*/
void rvRedRoverGameState::PlayerDeath( idPlayer* dead, idPlayer* killer ) {
	rvRoundGameState::PlayerDeath( dead, killer );

	if ( gameLocal.isClient || dead == NULL || !RoundIsLive() ) {
		return;
	}

	// suicides and world deaths do not hand anyone over
	if ( killer == NULL || killer == dead ) {
		return;
	}

	if ( killer->team < 0 || killer->team >= TEAM_MAX || dead->team == killer->team ) {
		return;
	}

	dead->GetUserInfo()->Set( "ui_team", gameLocal.mpGame.teamNames[ killer->team ] );
	cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "updateUI %d\n", dead->entityNumber ) );

	gameLocal.mpGame.CenterPrint( dead->entityNumber, "#str_41360", idMultiplayerGame::CPARM_TEAM, killer->team );
}

/*
================
rvRedRoverGameState::CheckRoundEnd

The round is over once everybody is wearing the same colours.
================
*/
bool rvRedRoverGameState::CheckRoundEnd( int &winningTeam ) {
	int marine = CountLiveTeamPlayers( TEAM_MARINE );
	int strogg = CountLiveTeamPlayers( TEAM_STROGG );

	if ( marine + strogg < 2 ) {
		// not enough players to decide anything
		return false;
	}

	if ( marine > 0 && strogg > 0 ) {
		return false;
	}

	winningTeam = marine ? TEAM_MARINE : TEAM_STROGG;
	return true;
}

/*
================
rvRedRoverGameState::ResolveRoundTimeout
================
*/
int rvRedRoverGameState::ResolveRoundTimeout( void ) {
	int marine = CountLiveTeamPlayers( TEAM_MARINE );
	int strogg = CountLiveTeamPlayers( TEAM_STROGG );

	if ( marine == strogg ) {
		return TEAM_NONE;
	}

	return ( marine > strogg ) ? TEAM_MARINE : TEAM_STROGG;
}

/*
================
rvRedRoverGameState::CheckMatchEnd

Quake Live counts Red Rover's round limit against the total number of rounds
played rather than either team's tally, because sides change constantly and a
per-team count means very little here.
================
*/
bool rvRedRoverGameState::CheckMatchEnd( int &winningTeam ) {
	int roundLimit, marine, strogg;

	winningTeam = TEAM_NONE;

	marine = gameLocal.mpGame.GetScoreForTeam( TEAM_MARINE );
	strogg = gameLocal.mpGame.GetScoreForTeam( TEAM_STROGG );

	roundLimit = gameLocal.serverInfo.GetInt( "si_roundLimit" );
	if ( roundLimit > 0 && ( marine + strogg ) >= roundLimit ) {
		winningTeam = ( marine > strogg ) ? TEAM_MARINE : ( ( strogg > marine ) ? TEAM_STROGG : TEAM_NONE );
		return true;
	}

	if ( gameLocal.mpGame.TimeLimitHit() ) {
		gameLocal.mpGame.PrintMessageEvent( -1, MSG_TIMELIMIT );
		winningTeam = ( marine > strogg ) ? TEAM_MARINE : ( ( strogg > marine ) ? TEAM_STROGG : TEAM_NONE );
		return true;
	}

	return false;
}

/*
================
rvRedRoverGameState::IsType
================
*/
bool rvRedRoverGameState::IsType( gameStateType_t t ) const {
	return ( t == rvRedRoverGameState::type );
}

/*
================
rvRedRoverGameState::GetClassType
================
*/
gameStateType_t rvRedRoverGameState::GetClassType( void ) {
	return rvRedRoverGameState::type;
}
