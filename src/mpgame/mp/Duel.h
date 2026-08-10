//----------------------------------------------------------------
// Duel.h
//
// openQ4 Duel, carried over from Quake Live.
//
// Quake 4 already has a mode called Tourney, but it is a different game: up
// to eight concurrent arenas running a single-elimination bracket.  Quake
// Live's Duel is a flat 1v1 with a spectator queue - the winner stays on, the
// loser goes to the back of the line.  Both are worth having, so Duel is its
// own gametype rather than a reshaping of Tourney.
//----------------------------------------------------------------

#ifndef __DUEL_H__
#define __DUEL_H__

#include "GameState.h"

class rvDuelGameState : public rvDMGameState {
public:
					rvDuelGameState( bool allocPrevious = true );

	virtual void	Clear( void );
	virtual void	Run( void );
	virtual bool	NewState( mpGameState_t newState );

	virtual void	ClientDisconnect( idPlayer* player );

	// Only the two people holding the arena may be in it.  Without this the
	// ordinary respawn path spawns every waiting player into the map and the
	// force-spectate loop below takes them straight back out, once per respawn
	// cycle - a full spawn each time, telefragging whatever is on the pad.
	virtual bool	AllowRespawn( idPlayer* player );

	virtual	bool	IsType( gameStateType_t type ) const;
	static gameStateType_t GetClassType( void );

	// player numbers of everyone waiting for a turn, in arrival order
	int				GetQueuePosition( int clientNum ) const;
	int				GetQueueLength( void ) const { return queue.Num(); }
	bool			IsContender( int clientNum ) const {
						return ( clientNum >= 0 &&
							( clientNum == contenders[ 0 ] || clientNum == contenders[ 1 ] ) );
					}

private:
	void			UpdateQueue( void );
	void			PromoteFromQueue( void );
	void			RotateLoser( void );

	// the two players currently holding the arena
	int				contenders[ 2 ];
	idList<int>		queue;

	static gameStateType_t type;
};

#endif // __DUEL_H__
