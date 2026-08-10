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
//
// This file is only concerned with HOW a bot sees, moves, aims and shoots.
// HOW WELL and IN WHAT MANNER it does any of that is not code: it is resolved
// into one flat botTraits_t per bot per spawn by rvBotCharacterManager, out of
// a skill curve, a play style and a named character, all of which are content
// files in the pak.  See BotCharacter.h.
//----------------------------------------------------------------

#ifndef __GAME_MP_BOT_H__
#define __GAME_MP_BOT_H__

// rvBot holds a botTraits_t by value and a character pointer, so the contract
// header has to be complete here rather than forward declared.  Game_local.h
// includes it immediately before this file too; the include guard makes saying
// so twice free, and it keeps Bot.h readable on its own.
#include "BotCharacter.h"

class idPlayer;
class idItem;

//----------------------------------------------------------------
// What the bot is currently trying to do.  Combat overrides navigation for
// aiming and firing but not for movement: a bot still routes to a goal while
// shooting at whatever it can see.
//----------------------------------------------------------------
typedef enum {
	BOTGOAL_NONE,
	BOTGOAL_ROAM,				// no reason to be anywhere in particular
	BOTGOAL_ITEM,				// heading for a pickup
	BOTGOAL_ENEMY,				// closing on a player
	BOTGOAL_OBJECTIVE			// flag, carrier, rescue, or control point
} botGoalType_t;

//----------------------------------------------------------------
// The ways a bot can get a combat decision wrong.  One is picked at a time on
// a roll against botTraits_t::mistakeChance and lasts mistakeMsec.
//
// These are deliberately things a player does under pressure, not handicaps.
// A bot that simply aimed worse would read as broken; a bot that loses its
// target for half a second, or pulls the trigger before the sight has stopped
// moving, reads as beatable.
//----------------------------------------------------------------
typedef enum {
	BOTMISTAKE_NONE,
	BOTMISTAKE_LOSETRACK,		// the believed target position stops being updated
	BOTMISTAKE_MISTIMEDSHOT,	// fires without waiting for the aim to settle
	BOTMISTAKE_WRONGWEAPON,		// keeps whatever is in hand instead of switching
	BOTMISTAKE_LATEDODGE,		// takes twice as long to start dodging incoming fire
	BOTMISTAKE_NUM
} botMistake_t;

//----------------------------------------------------------------
// rvBot
//----------------------------------------------------------------
class rvBot {
public:
							rvBot( void );

	// skillOverride is the optional per-bot level from "addbot <name> <skill>".
	// -1 means "follow bot_skill", which is what every other caller wants.
	void					Init( int clientNum, const char *name, int skillOverride = -1,
							  bool requireExactCharacter = false );
	void					Shutdown( void );

	// Re-point this bot at the character of the same name in a freshly parsed
	// roster.  Every pointer handed out before a reload dangles, so this has to
	// run for every live bot before anything reads a personality again.
	void					RebindCharacter( void );

	bool					IsActive( void ) const { return active; }
	int						GetClientNum( void ) const { return clientNum; }
	const char *			GetName( void ) const { return name.c_str(); }

	const rvBotCharacter *	GetCharacter( void ) const { return character; }
	const botTraits_t &		GetTraits( void ) const { return traits; }

	// The integer level the personality was resolved from - bot_skill, or this
	// bot's own override - and the fractional level it actually plays at once
	// bot_skillVariance has had its say.  Both are reported by botlist so what
	// the operator reads is what the bot is using.
	int						GetSkillLevel( void ) const { return skillLevel; }
	float					GetEffectiveSkill( void ) const { return effectiveSkill; }

	// Fill in this bot's identity so any userinfo update keeps it in the game
	// instead of dropping it back to the join menu.
	void					FillUserInfo( idDict &info );

	// One server frame.  Writes the client's user command in place.
	void					Think( usercmd_t &cmd );

	// Called when the player entity behind this bot respawns.
	void					OnSpawn( void );

	// -- chat, all server side and all driven from rvBotManager --
	//
	// Queued rather than sent: a bot that answers a frag on the frame it landed
	// reads as a script.  A pending line is dropped if the bot leaves.
	void					QueueChat( rvBotChatEvent event, const char *other, const char *weapon, const char *item );
	bool					TryQueueReply( const idStr &normalizedText, bool sourceIsBot, bool addressed,
									   const char *other, bool teamOnly );
	void					SayFarewell( void );

	void					OnKilledEnemy( idPlayer *victim, int methodOfDeath );
	void					OnKilledBy( idPlayer *killer, int methodOfDeath );
	void					OnDamaged( idPlayer *attacker, int damage, const idVec3 &dir );
	void					OnMatchStart( void );
	void					OnMatchEnd( bool won );

	// Move every absolute deadline forward by one frozen competitive-pause
	// frame.  gameLocal.time advances through a pause; this bot does not think.
	void					ShiftMatchTime( int deltaMsec );

	// Read-only coordination state used by rvBotManager to keep a whole team
	// from reserving the same pickup or support target.
	idEntity *				GetGoalEntity( void ) const { return goalEntity.GetEntity(); }
	int						GetGoalType( void ) const { return goalType; }

private:
	friend class rvBotManager;

	// Resolves the personality.  Called once per spawn, never per frame, so a
	// mid-match bot_skill edit lands on the next respawn instead of retuning a
	// bot in the middle of a fight it is already losing.
	void					ResolveTraits( void );

	idPlayer *				GetPlayer( void ) const;

	void					UpdateEnemy( idPlayer *self );
	void					UpdateGoal( idPlayer *self );
	void					UpdateMovement( idPlayer *self, usercmd_t &cmd );
	void					UpdateAim( idPlayer *self, usercmd_t &cmd );
	void					UpdateWeapon( idPlayer *self );
	void					UpdateFire( idPlayer *self, usercmd_t &cmd );
	void					UpdateChat( idPlayer *self );

	// Everything that happens the moment a target registers: the reaction
	// deadline, the peripheral penalty, the mistake roll and the belief reset.
	void					AcquireEnemy( idPlayer *self, idPlayer *foe, bool fresh );

	void					RollMistake( void );
	bool					MistakeActive( int mistake ) const;

	// Live MP-resolved projectile launch data.  Hitscan/melee return zero speed;
	// arcing projectiles return both speed and gravity for ballistic aim.
	float					ProjectileSpeed( idPlayer *self ) const;
	idVec3					ProjectileGravity( idPlayer *self ) const;
	bool					BurstAllows( idPlayer *self );

	float					TargetScore( idPlayer *other, float distSqr ) const;
	idVec3					EnemyPursuitOrigin( void ) const;
	bool					WantsHealth( idPlayer *self ) const;
	bool					AtObjectiveHoldPosition( idPlayer *self ) const;
	bool					IsDodging( idPlayer *self ) const;
	void					TrackDamage( idPlayer *self );
	void					ScheduleDodge( const idVec3 &threatDirection );

	bool					CanSee( idPlayer *self, idEntity *other, idVec3 *visiblePoint = NULL ) const;
	bool					IsEnemy( idPlayer *self, idPlayer *other ) const;

	bool					Repath( idPlayer *self, const idVec3 &goal, bool preserveProgress = false );
	bool					AdvancePath( idPlayer *self );
	idEntity *				PickItemGoal( idPlayer *self, rvNavPath &goalPath, float &goalUtility );
	float					ItemUtility( idPlayer *self, idItem *item ) const;
	float					PathDistanceRemaining( const idVec3 &origin ) const;
	void					AbandonGoal( int forMsec );
	bool					RecoverToNavMesh( idPlayer *self, usercmd_t &cmd );
	bool					IsAvoided( const idEntity *ent ) const;
	void					Avoid( idEntity *ent, int forMsec );
	void					ResetTraversal( void );

	void					ApplyMove( const idVec3 &moveDir, usercmd_t &cmd ) const;
	void					PressJump( usercmd_t &cmd ) const;

// openQ4 BEGIN
	// Liquid handling. The navmesh is generated against solids only, so liquid volumes are
	// invisible to routing and all of this has to work locally, off point queries.
	int						LiquidAtFeet( void ) const;
	int						LiquidAtEye( void ) const;
	void					UpdateLiquidMovement( idPlayer *self, usercmd_t &cmd );
	idVec3					AvoidLiquidHazard( const idVec3 &moveDir ) const;
// openQ4 END

	int						clientNum;
	idStr					name;
	bool					active;
	bool					initialTeamAssignmentPending;	// ignore inherited slot userinfo once
	int						teamAssignment;		// pre-spawn reservation, TEAM_NONE when unset
	int						teamAssignmentInstance;

	// -- personality --
	const rvBotCharacter *	character;			// NULL with bot_characters 0, or an empty roster
	botTraits_t				traits;
	int						skillOverride;		// per-bot level from addbot, -1 for bot_skill
	int						skillLevel;			// integer level the traits were resolved from
	float					effectiveSkill;		// that level after bot_skillVariance

	// -- navigation --
	rvNavPath				path;
	int						pathCorner;
	int						pathTime;			// when the current route was built
	idVec3					goalOrigin;
	idEntityPtr<idEntity>	goalEntity;
	int						goalType;
	int						goalGiveUpTime;		// abandon a goal that is not working out
	float					goalUtility;			// benefit used for hysteresis and coordination
	int						goalCommitUntil;		// do not chatter between similar choices
	float					goalBestDistance;		// route progress extends a sensible deadline
	int						goalProgressTime;
	int						objectiveKind;			// botObjectiveKind_t for objective refreshes
	bool					objectiveHoldPosition;
	int						repathFailures;
	int						enemyPathTime;		// last attempt to route to an enemy, successful or not
	int						nextGoalSelectTime;	// throttles the full goal scan
	int						holdUntil;			// a patient bot standing still on a finished wander
	int						noRouteSince;		// pathTime is route age, not recovery age

	// A non-walk link is an action, not an ordinary proximity corner.  The bot
	// approaches its source, commits to the action, and only advances after it
	// has actually reached the destination side.
	int						traversalCorner;
	idVec3					traversalStartOrigin;
	int						traversalStartTime;
	bool					traversalEntered;
	bool					traversalStarted;

	// Off-mesh recovery target, cached because finding it is a wide search.
	idVec3					recoverTarget;
	int						recoverSearchTime;
	bool					recoverValid;

	// With nowhere known to head for, the bot picks a heading and keeps it for
	// a while.  Turning a fixed amount every frame is a per-frame rate, and it
	// only ever walks the bot in a circle it cannot escape.
	int						recoverWanderUntil;
	float					recoverWanderYaw;

	// Goals that did not work out, so the bot does not ping-pong between two
	// items it cannot take.  One slot would only ever remember the last one.
	static const int		MAX_AVOID_GOALS = 8;
	struct avoidGoal_s {
		idEntityPtr<idEntity>	ent;
		int						until;
	}						avoidGoals[MAX_AVOID_GOALS];
	int						avoidNext;

	// -- stuck detection --
	// Progress is measured against the route, not against displacement: a bot
	// that is moving but not getting closer to its corner is stuck, and one
	// that is standing still already fails the same test.  There is deliberately
	// no "has it physically moved" member here - it read as a second signal and
	// was never anything of the kind.
	int						stuckTime;
	int						unstickUntil;
	float					unstickSide;
	int						stuckChecks;
	int						stuckPathCorner;
	float					stuckCornerDistance;

	// -- combat --
	idEntityPtr<idPlayer>	enemy;
	int						enemyAcquiredTime;
	int						enemyLastSeenTime;
	idVec3					enemyLastSeenOrigin;
	idVec3					enemyLastSeenVelocity;
	idVec3					enemyVisiblePoint;
	bool					targetPickBest;		// this engagement's targetSelection roll
	idEntityPtr<idPlayer>	lastAttacker;
	int						lastAttackerTime;

	// -- aim --
	// The bot aims at where it BELIEVES the target is, which trails where the
	// target actually is by aimTrackTimeConst.  aimPoint is that belief plus
	// the projectile lead, and it is what both the view and the fire cone are
	// measured against - if the two disagreed the bot would aim at the lead
	// point and then refuse to shoot because the centre was outside the cone.
	idAngles				aimAngles;
	idAngles				aimRate;			// degrees/second, the slew's own velocity
	idVec3					aimBelief;
	bool					aimBeliefValid;
	idVec3					aimPoint;
	bool					aimPointValid;
	idAngles				aimTrackAngles;		// last frame's angles to the aim point
	bool					aimTrackValid;
	float					aimAngVel;			// degrees/second, low passed
	float					aimLeadRoll;		// this burst's lead error multiplier
	float					tremorPhase[4];		// seeded from clientNum so no two bots shake in step

	// -- trigger --
	int						enemyFireTime;		// reaction deadline, set once per acquisition
	int						onTargetTime;		// how long the aim has sat inside the cone
	int						burstEndTime;
	int						burstRestTime;
	int						nextSingleShotTime;
	bool					wasFiring;			// so bot_debugAim logs the shot, not the frame

	// -- mistakes --
	int						mistake;
	int						mistakeEndTime;
	int						nextMistakeTime;

	// -- dodging --
	int						damageStamp;		// last idPlayer::lastDmgTime this bot reacted to
	int						dodgeStartTime;
	int						dodgeEndTime;
	int						nextThreatScanTime;
	float					strafeSide;
	int						strafeFlipTime;
	bool					dodgeJump;

	// -- chat --
	idStr					chatPending;
	bool					chatTeamOnly;
	bool					chatPendingIsReply;
	int						chatSendTime;
	int						killStreak;
	int						revengeTarget;		// client number owed a debt, -1 for none
	bool					announcedEntry;

	int						nextWeaponTime;
	int						lastWeaponSwitchTime;
	int						nextRejoinTime;		// throttles the leave-spectate request
	int						nextStatusTime;		// bot_debug 2 throttle
	int						nextAimDebugTime;	// bot_debugAim 2 throttle
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

	// Console entry points.  skillLevel is the optional per-bot override from
	// "addbot <name> <skill>"; -1 means follow bot_skill.
	bool					AddBot( const char *name, int skillLevel = -1,
								bool requireExactCharacter = false );
	bool					RemoveBot( const char *name );
	void					RemoveAll( void );
	void					ListBots( void ) const;

	// Re-bind every live bot after the character roster has been reparsed.
	// botreload must call this before the next frame runs, or every bot is
	// reading a freed character.
	void					RebindCharacters( void );

	// Lifecycle hooks.
	void					OnSpawnPlayer( int clientNum, bool isBot, const char *botName );
	void					OnClientDisconnect( int clientNum );
	void					OnMapShutdown( void );

	// Match hooks, all server side.  OnMatchStart/OnMatchEnd belong on the
	// GAMEON and GAMEREVIEW transitions in rvGameState::NewState; OnPlayerDeath
	// belongs beside statManager->Kill in idMultiplayerGame::PlayerDeath, which
	// is after scoring has been committed, so a bot reacting there already sees
	// the post-frag board.
	void					OnMatchStart( void );
	void					OnMatchEnd( void );
	void					OnPlayerDeath( idPlayer *dead, idPlayer *killer, int methodOfDeath );

	// Called once per frozen frame from
	// idMultiplayerGame::RebaseCompetitivePauseFrame.  idGameLocal::RunFrame
	// keeps advancing gameLocal.time through a competitive pause while it skips
	// rvBotManager::Think, so without this every bot resumes with every deadline
	// already expired: reactions forgiven, goals abandoned, stuck checks fired.
	void					ShiftMatchTime( int deltaMsec );
	void					OnPlayerDamaged( idPlayer *victim, idEntity *attacker, int damage, const idVec3 &dir );

	// Called once for an accepted typed-chat line on the server.  It chooses at
	// most one visible bot to answer and lets that bot's character content,
	// chatiness, delay and the shared flood throttle decide whether it speaks.
	void					OnChatMessage( int sourceClientNum, bool teamOnly, const char *visibleText );

	bool					IsBot( int clientNum ) const;
	int						NumBots( void ) const;
	int						GoalClaimCount( const rvBot *requester, const idEntity *goal ) const;
	void					FillUserInfo( int clientNum, idDict &info );

	// Selects the least-populated side in the bot's gameplay instance.  The
	// count includes active bot reservations whose idPlayer has not spawned yet.
	int						BalancedTeamForBot( int clientNum, int &balanceInstance ) const;

	// Builds the navmesh if it has not been built for this map yet.  Returns
	// false when the map has no walkable ground the bots can use.
	bool					EnsureNavMesh( void );

private:
	void					CheckMinPlayers( void );
	void					UpdateLeaderChat( void );
	void					ResetReplyCooldowns( void );
	const char *			PickBotName( void ) const;

	rvBot					bots[MAX_CLIENTS];
	bool					navBuilt;
	bool					navFailed;
	int						nextMinPlayerCheck;

	// AddBot allocates the client slot and the engine calls straight back into
	// OnSpawnPlayer, which is where the bot is actually built - so a per-bot
	// skill has to be parked here for the length of that call.
	int						pendingSkill;
	bool					pendingExactCharacter;

	// Who is winning.  idMultiplayerGame::UpdateLeader is declared and never
	// defined and the lead announcements in CommonRun are written for the local
	// player only, so there is no server side hook to hang lead chat on.
	int						leaderClientNum;
	int						nextLeaderCheck;
	int						nextReplySourceTime[MAX_CLIENTS];
};

extern rvBotManager			botManager;

#endif /* !__GAME_MP_BOT_H__ */
