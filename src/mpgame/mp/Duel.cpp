//----------------------------------------------------------------
// Duel.cpp
//
// openQ4 Duel, carried over from Quake Live.
//----------------------------------------------------------------

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "Duel.h"

gameStateType_t rvDuelGameState::type = GS_DUEL;

/*
================
rvDuelGameState::rvDuelGameState
================
*/
rvDuelGameState::rvDuelGameState( bool allocPrevious ) : rvDMGameState( allocPrevious ) {
	Clear();

	trackPrevious = allocPrevious;
}

/*
================
rvDuelGameState::Clear
================
*/
void rvDuelGameState::Clear( void ) {
	rvGameState::Clear();

	contenders[ 0 ] = -1;
	contenders[ 1 ] = -1;
	queue.Clear();
}

/*
================
rvDuelGameState::GetQueuePosition

1 for next up, 2 for the one after, and so on.  0 means not queued.
================
*/
int rvDuelGameState::GetQueuePosition( int clientNum ) const {
	int i;

	for ( i = 0; i < queue.Num(); i++ ) {
		if ( queue[ i ] == clientNum ) {
			return i + 1;
		}
	}

	return 0;
}

/*
================
rvDuelGameState::UpdateQueue

Keeps the waiting list honest: everyone who wants to play and is not one of
the two contenders joins the back of the queue, and anyone who has left or
gone to spectate by choice drops off it.
================
*/
void rvDuelGameState::UpdateQueue( void ) {
	int i;

	// drop stale entries
	for ( i = queue.Num() - 1; i >= 0; i-- ) {
		idEntity *ent = gameLocal.entities[ queue[ i ] ];

		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ||
			 static_cast< idPlayer * >( ent )->wantSpectate ) {
			queue.RemoveIndex( i );
		}
	}

	for ( i = 0; i < 2; i++ ) {
		if ( contenders[ i ] < 0 ) {
			continue;
		}

		idEntity *ent = gameLocal.entities[ contenders[ i ] ];
		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ||
			 static_cast< idPlayer * >( ent )->wantSpectate ) {
			contenders[ i ] = -1;
		}
	}

	// enrol anyone new who wants in
	for ( i = 0; i < gameLocal.numClients; i++ ) {
		idEntity *ent = gameLocal.entities[ i ];

		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}

		idPlayer *p = static_cast< idPlayer * >( ent );
		if ( p->wantSpectate ) {
			continue;
		}

		if ( i == contenders[ 0 ] || i == contenders[ 1 ] ) {
			continue;
		}

		if ( GetQueuePosition( i ) == 0 ) {
			queue.Append( i );
		}
	}
}

/*
================
rvDuelGameState::PromoteFromQueue
================
*/
void rvDuelGameState::PromoteFromQueue( void ) {
	int i;

	for ( i = 0; i < 2 && queue.Num() > 0; i++ ) {
		if ( contenders[ i ] >= 0 ) {
			continue;
		}

		contenders[ i ] = queue[ 0 ];
		queue.RemoveIndex( 0 );
	}
}

/*
================
rvDuelGameState::RotateLoser

Winner stays on.  When a match ends the loser goes to the back of the queue
and the next challenger takes their place.
================
*/
void rvDuelGameState::RotateLoser( void ) {
	idPlayer *first, *second, *loser;

	if ( contenders[ 0 ] < 0 || contenders[ 1 ] < 0 ) {
		return;
	}

	// nobody waiting means the same pair simply plays again
	if ( queue.Num() == 0 ) {
		return;
	}

	first = static_cast< idPlayer * >( gameLocal.entities[ contenders[ 0 ] ] );
	second = static_cast< idPlayer * >( gameLocal.entities[ contenders[ 1 ] ] );

	if ( first == NULL || second == NULL ) {
		return;
	}

	if ( gameLocal.mpGame.GetScore( first ) == gameLocal.mpGame.GetScore( second ) ) {
		// a drawn duel puts both back in line, as Quake Live does
		queue.Append( contenders[ 0 ] );
		queue.Append( contenders[ 1 ] );
		contenders[ 0 ] = -1;
		contenders[ 1 ] = -1;
		return;
	}

	if ( gameLocal.mpGame.GetScore( first ) < gameLocal.mpGame.GetScore( second ) ) {
		loser = first;
		contenders[ 0 ] = -1;
	} else {
		loser = second;
		contenders[ 1 ] = -1;
	}

	queue.Append( loser->entityNumber );
}

/*
================
rvDuelGameState::NewState
================
*/
void rvDuelGameState::NewState( mpGameState_t newState ) {
	rvDMGameState::NewState( newState );

	if ( gameLocal.isClient ) {
		return;
	}

	if ( newState == GAMEREVIEW ) {
		RotateLoser();
	}
}

/*
================
rvDuelGameState::ClientDisconnect
================
*/
void rvDuelGameState::ClientDisconnect( idPlayer* player ) {
	int i;

	rvDMGameState::ClientDisconnect( player );

	if ( player == NULL ) {
		return;
	}

	for ( i = 0; i < 2; i++ ) {
		if ( contenders[ i ] == player->entityNumber ) {
			contenders[ i ] = -1;
		}
	}

	i = GetQueuePosition( player->entityNumber );
	if ( i > 0 ) {
		queue.RemoveIndex( i - 1 );
	}
}

/*
================
rvDuelGameState::Run

Everything above the queue is plain deathmatch: Duel is deathmatch with the
arena restricted to two people at a time.
================
*/
void rvDuelGameState::Run( void ) {
	int i;

	rvDMGameState::Run();

	if ( gameLocal.isClient ) {
		return;
	}

	UpdateQueue();

	// only fill empty seats while a match is being set up, so a duel is never
	// interrupted by a third player walking in
	if ( currentState == WARMUP || currentState == INACTIVE || currentState == NEXTGAME ) {
		PromoteFromQueue();
	}

	// hold everyone who is not a contender in spectator
	for ( i = 0; i < gameLocal.numClients; i++ ) {
		idEntity *ent = gameLocal.entities[ i ];

		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}

		idPlayer *p = static_cast< idPlayer * >( ent );
		if ( p->wantSpectate ) {
			continue;
		}

		bool isContender = ( i == contenders[ 0 ] || i == contenders[ 1 ] );

		if ( !isContender && !p->spectating ) {
			p->ServerSpectate( true );
		}
	}
}

/*
================
rvDuelGameState::IsType
================
*/
bool rvDuelGameState::IsType( gameStateType_t t ) const {
	return ( t == rvDuelGameState::type );
}

/*
================
rvDuelGameState::GetClassType
================
*/
gameStateType_t rvDuelGameState::GetClassType( void ) {
	return rvDuelGameState::type;
}
