//----------------------------------------------------------------
// Bot.h
//
// Multiplayer bots for openQ4.
//
// A bot is not an entity.  It occupies a real client slot, spawns a real
// idPlayer, and is driven by writing the user command the engine would have
// received from a remote player.  Everything downstream - movement physics,
// weapons, scoring, the scoreboard, game type rules - therefore treats it as
// an ordinary player with no special cases.
//
// Navigation comes from rvNavMesh, which is generated from the collision world
// at map load, so bots work on any map without an offline compile step.
//----------------------------------------------------------------

#ifndef __GAME_MP_BOT_H__
#define __GAME_MP_BOT_H__

class idPlayer;

//----------------------------------------------------------------
// What the bot is currently trying to do.  Combat overrides navigation for
// aiming and firing but not for movement: a bot still routes to a goal while
// shooting at whatever it can see.
//----------------------------------------------------------------
typedef enum {
	BOTGOAL_NONE,
	BOTGOAL_ROAM,				// no reason to be anywhere in particular
	BOTGOAL_ITEM,				// heading for a pickup
	BOTGOAL_ENEMY				// closing on a player
} botGoalType_t;

//----------------------------------------------------------------
// Everything difficulty scales.  Resolved from bot_skill once per spawn so a
// mid-match change of the cvar does not make bots twitch.
//----------------------------------------------------------------
typedef struct botSkill_s {
	float					turnSpeed;			// degrees per second the view can slew
	float					aimError;			// degrees of steady state aim offset
	int						reactionMsec;		// delay between seeing an enemy and firing at it
	float					sightRange;			// how far the bot notices players
	float					strafeChance;		// 0..1, how much it dodges while fighting
} botSkill_t;

//----------------------------------------------------------------
// rvBot
//----------------------------------------------------------------
class rvBot {
public:
							rvBot( void );

	void					Init( int clientNum, const char *name );
	void					Shutdown( void );

	bool					IsActive( void ) const { return active; }
	int						GetClientNum( void ) const { return clientNum; }
	const char *			GetName( void ) const { return name.c_str(); }

	// Fill in this bot's identity so any userinfo update keeps it in the game
	// instead of dropping it back to the join menu.
	void					FillUserInfo( idDict &info ) const;

	// One server frame.  Writes the client's user command in place.
	void					Think( usercmd_t &cmd );

	// Called when the player entity behind this bot respawns.
	void					OnSpawn( void );

private:
	void					ResolveSkill( void );

	idPlayer *				GetPlayer( void ) const;

	void					UpdateEnemy( idPlayer *self );
	void					UpdateGoal( idPlayer *self );
	void					UpdateMovement( idPlayer *self, usercmd_t &cmd );
	void					UpdateAim( idPlayer *self, usercmd_t &cmd );
	void					UpdateWeapon( idPlayer *self );
	void					UpdateFire( idPlayer *self, usercmd_t &cmd );

	bool					CanSee( idPlayer *self, idEntity *other ) const;
	bool					IsEnemy( idPlayer *self, idPlayer *other ) const;

	bool					Repath( idPlayer *self, const idVec3 &goal );
	bool					AdvancePath( idPlayer *self );
	idEntity *				PickItemGoal( idPlayer *self );
	void					AbandonGoal( int forMsec );
	bool					RecoverToNavMesh( idPlayer *self, usercmd_t &cmd );
	bool					IsAvoided( const idEntity *ent ) const;
	void					Avoid( idEntity *ent, int forMsec );

	void					ApplyMove( const idVec3 &moveDir, usercmd_t &cmd ) const;
	void					PressJump( usercmd_t &cmd ) const;

	int						clientNum;
	idStr					name;
	bool					active;

	botSkill_t				skill;
	int						skillLevel;			// bot_skill this was resolved from

	// -- navigation --
	rvNavPath				path;
	int						pathCorner;
	int						pathTime;			// when the current route was built
	idVec3					goalOrigin;
	idEntityPtr<idEntity>	goalEntity;
	int						goalType;
	int						goalGiveUpTime;		// abandon a goal that is not working out
	int						repathFailures;
	int						enemyPathTime;		// last attempt to route to an enemy, successful or not
	int						nextGoalSelectTime;	// throttles the full goal scan

	// Off-mesh recovery target, cached because finding it is a wide search.
	idVec3					recoverTarget;
	int						recoverSearchTime;
	bool					recoverValid;

	// Goals that did not work out, so the bot does not ping-pong between two
	// items it cannot take.  One slot would only ever remember the last one.
	static const int		MAX_AVOID_GOALS = 8;
	struct avoidGoal_s {
		idEntityPtr<idEntity>	ent;
		int						until;
	}						avoidGoals[MAX_AVOID_GOALS];
	int						avoidNext;

	// -- stuck detection --
	idVec3					stuckOrigin;
	int						stuckTime;
	int						unstickUntil;
	float					unstickSide;

	// -- combat --
	idEntityPtr<idPlayer>	enemy;
	int						enemyAcquiredTime;
	int						enemyLastSeenTime;
	idVec3					enemyLastSeenOrigin;
	idAngles				aimAngles;
	idAngles				aimOffset;
	int						aimOffsetTime;

	int						nextWeaponTime;
	int						nextRejoinTime;		// throttles the leave-spectate request
	int						nextStatusTime;		// bot_debug 2 throttle
};

//----------------------------------------------------------------
// rvBotManager
//
// Owns the bot slots and is the only thing the rest of the game talks to.
//----------------------------------------------------------------
class rvBotManager {
public:
							rvBotManager( void );

	void					Init( void );
	void					Shutdown( void );

	// Called once per server frame from idGameLocal::RunFrame, before any
	// entity thinks, so the bots' commands are in place when players read them.
	void					Think( void );

	// Console entry points.
	bool					AddBot( const char *name );
	bool					RemoveBot( const char *name );
	void					RemoveAll( void );
	void					ListBots( void ) const;

	// Lifecycle hooks.
	void					OnSpawnPlayer( int clientNum, bool isBot, const char *botName );
	void					OnClientDisconnect( int clientNum );
	void					OnMapShutdown( void );

	bool					IsBot( int clientNum ) const;
	int						NumBots( void ) const;
	void					FillUserInfo( int clientNum, idDict &info ) const;

	// Builds the navmesh if it has not been built for this map yet.  Returns
	// false when the map has no walkable ground the bots can use.
	bool					EnsureNavMesh( void );

private:
	void					CheckMinPlayers( void );
	const char *			PickBotName( void ) const;

	rvBot					bots[MAX_CLIENTS];
	bool					navBuilt;
	bool					navFailed;
	int						nextMinPlayerCheck;
};

extern rvBotManager			botManager;

#endif /* !__GAME_MP_BOT_H__ */
