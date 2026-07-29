//----------------------------------------------------------------
// Bot.cpp
//
// Multiplayer bot behaviour and user command synthesis.  See Bot.h.
//----------------------------------------------------------------

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "Bot.h"

rvBotManager				botManager;

/*
===============================================================================

	Tuning.

===============================================================================
*/

static const int	BOT_REPATH_COMBAT_MSEC	= 750;		// chasing a player that keeps moving
static const int	BOT_REPATH_IDLE_MSEC	= 3000;		// heading for a fixed goal
static const float	BOT_CORNER_REACH		= 28.0f;	// horizontal distance that counts as "arrived"
static const float	BOT_CORNER_REACH_Z		= 72.0f;	// and how far off in height that may be
static const int	BOT_ENEMY_MEMORY_MSEC	= 3000;		// how long a lost enemy is still chased
static const float	BOT_ITEM_SEARCH_RANGE	= 2500.0f;
static const float	BOT_COMBAT_RANGE		= 500.0f;	// preferred distance from the enemy
static const float	BOT_FIRE_CONE_DOT		= 0.96f;	// ~16 degrees, fire once aim is this close
static const int	BOT_AIM_JITTER_MSEC		= 400;		// how often the steady state aim error is re-rolled
static const int	BOT_WEAPON_CHECK_MSEC	= 500;
static const int	BOT_STUCK_CHECK_MSEC	= 700;
static const float	BOT_STUCK_DISTANCE		= 24.0f;
static const int	BOT_UNSTICK_MSEC		= 900;
static const int	BOT_GOAL_GIVEUP_MSEC	= 12000;	// a goal this stale is not going to resolve
static const int	BOT_GOAL_AVOID_MSEC		= 20000;	// and how long an abandoned goal stays off the list
static const float	BOT_ITEM_ARRIVED		= 64.0f;	// close enough that a pickup would have fired
static const int	BOT_GOAL_SELECT_MSEC	= 250;		// how often the full goal scan may run
static const int	BOT_RECOVER_SEARCH_MSEC	= 500;		// how often the off-mesh search may run
static const float	BOT_RECOVER_RADIUS		= 1536.0f;	// how far off the mesh a bot will look for a way back
static const int	BOT_JUMP_PHASE_MSEC		= 120;		// jump button on/off half period
static const int	BOT_RECOVER_IDLE_MSEC	= 2000;		// no route this long means the bot is stranded
static const int	BOT_REACQUIRE_MSEC		= 500;		// out of sight this long counts as a new sighting

// Weapon preference, best first.  Two lists so a bot does not try to snipe
// with a shotgun or fight a knife range duel with a rail gun.
static const char *botWeaponsFar[] = {
	"weapon_railgun", "weapon_rocketlauncher", "weapon_lightninggun", "weapon_hyperblaster",
	"weapon_nailgun", "weapon_machinegun", "weapon_grenadelauncher", "weapon_shotgun",
	"weapon_blaster", "weapon_gauntlet", NULL
};

static const char *botWeaponsClose[] = {
	"weapon_rocketlauncher", "weapon_lightninggun", "weapon_shotgun", "weapon_hyperblaster",
	"weapon_nailgun", "weapon_railgun", "weapon_machinegun", "weapon_grenadelauncher",
	"weapon_blaster", "weapon_gauntlet", NULL
};

static const char *botNames[] = {
	"Voss", "Bidwell", "Cortez", "Rhodes", "Sledge", "Morris", "Strauss", "Tetzlaff",
	"Marsh", "Hollenbeck", "Sorg", "Anderson", "Gunner", "Makron", "Kane", "Bagby",
	NULL
};

/*
===============================================================================

	rvBot

===============================================================================
*/

/*
================
rvBot::rvBot
================
*/
rvBot::rvBot( void ) {
	clientNum			= -1;
	active				= false;
	skillLevel			= -1;

	memset( &skill, 0, sizeof( skill ) );

	pathCorner			= 0;
	pathTime			= 0;
	goalType			= BOTGOAL_NONE;
	goalOrigin.Zero();
	goalGiveUpTime		= 0;
	repathFailures		= 0;
	enemyPathTime		= 0;
	nextGoalSelectTime	= 0;
	recoverTarget.Zero();
	recoverSearchTime	= 0;
	recoverValid		= false;
	avoidNext			= 0;

	for ( int i = 0; i < MAX_AVOID_GOALS; i++ ) {
		avoidGoals[i].until = 0;
	}

	stuckOrigin.Zero();
	stuckTime			= 0;
	unstickUntil		= 0;
	unstickSide			= 1.0f;

	enemyAcquiredTime	= 0;
	enemyLastSeenTime	= 0;
	enemyLastSeenOrigin.Zero();
	aimAngles.Zero();
	aimOffset.Zero();
	aimOffsetTime		= 0;

	nextWeaponTime		= 0;
	nextRejoinTime		= 0;
	nextStatusTime		= 0;
}

/*
================
rvBot::Init
================
*/
void rvBot::Init( int clientNum, const char *name ) {
	this->clientNum	= clientNum;
	this->name		= ( name && name[0] ) ? name : "bot";
	this->active	= true;

	skillLevel = -1;
	ResolveSkill();

	OnSpawn();
}

/*
================
rvBot::Shutdown
================
*/
void rvBot::Shutdown( void ) {
	active		= false;
	clientNum	= -1;
	path.Clear();
	goalEntity	= NULL;
	enemy		= NULL;
}

/*
================
rvBot::ResolveSkill

bot_skill runs 1 (harmless) to 5 (unpleasant).  Everything in between is a
straight interpolation so the middle settings are actually distinct.
================
*/
void rvBot::ResolveSkill( void ) {
	const int level = idMath::ClampInt( 1, 5, bot_skill.GetInteger() );

	if ( level == skillLevel ) {
		return;
	}
	skillLevel = level;

	const float t = ( level - 1 ) / 4.0f;

	skill.turnSpeed		= 200.0f + t * 700.0f;
	skill.aimError		= 7.0f - t * 6.5f;
	skill.reactionMsec	= (int)( 450.0f - t * 380.0f );
	skill.sightRange	= 1400.0f + t * 1600.0f;
	skill.strafeChance	= 0.15f + t * 0.55f;
}

/*
================
rvBot::OnSpawn
================
*/
void rvBot::OnSpawn( void ) {
	ResolveSkill();

	path.Clear();
	pathCorner			= 0;
	pathTime			= 0;
	goalType			= BOTGOAL_NONE;
	goalEntity			= NULL;
	goalGiveUpTime		= 0;
	repathFailures		= 0;
	enemyPathTime		= 0;
	nextGoalSelectTime	= 0;
	recoverValid		= false;
	recoverSearchTime	= 0;
	avoidNext			= 0;

	for ( int i = 0; i < MAX_AVOID_GOALS; i++ ) {
		avoidGoals[i].until = 0;
	}

	enemy				= NULL;
	enemyAcquiredTime	= 0;
	enemyLastSeenTime	= 0;
	stuckTime			= gameLocal.time;
	unstickUntil		= 0;
	nextWeaponTime		= 0;

	idPlayer *self = GetPlayer();
	if ( self ) {
		aimAngles	= self->viewAngles;
		stuckOrigin	= self->GetPhysics()->GetOrigin();
	}
}

/*
================
rvBot::GetPlayer
================
*/
idPlayer *rvBot::GetPlayer( void ) const {
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return NULL;
	}

	idEntity *ent = gameLocal.entities[clientNum];
	if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
		return NULL;
	}

	return static_cast<idPlayer *>( ent );
}

/*
================
rvBot::FillUserInfo

The engine broadcasts a bot's user info with nothing but a name in it, and an
idPlayer with no ui_autoJoin sits in the join menu as a spectator forever.  So
every user info update for a bot is topped up here instead.
================
*/
void rvBot::FillUserInfo( idDict &info ) const {
	info.Set( "ui_name", name.c_str() );
	info.SetBool( "ui_autoJoin", true );
	info.SetBool( "ui_joined", true );
	info.Set( "ui_spectate", "Play" );
	info.Set( "ui_ready", "Ready" );
	info.Set( "ui_showGun", "1" );

	if ( !info.GetInt( "ui_handicap" ) ) {
		info.SetInt( "ui_handicap", 100 );
	}

	// Team games need a side before the player spawns, or the bot lands on
	// whatever team the server's own user info happened to name.
	if ( gameLocal.IsTeamGame() && !info.GetString( "ui_team" )[0] ) {
		int teamCount[TEAM_MAX];

		memset( teamCount, 0, sizeof( teamCount ) );

		for ( int i = 0; i < gameLocal.numClients; i++ ) {
			idEntity *ent = gameLocal.entities[i];
			if ( i == clientNum || !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
				continue;
			}
			idPlayer *player = static_cast<idPlayer *>( ent );
			if ( !player->spectating && player->team >= 0 && player->team < TEAM_MAX ) {
				teamCount[player->team]++;
			}
		}

		const int team = ( teamCount[1] < teamCount[0] ) ? 1 : 0;
		info.Set( "ui_team", idMultiplayerGame::teamNames[team] );
	}
}

/*
================
rvBot::IsEnemy
================
*/
bool rvBot::IsEnemy( idPlayer *self, idPlayer *other ) const {
	if ( !other || other == self ) {
		return false;
	}
	if ( other->spectating || other->health <= 0 ) {
		return false;
	}
	if ( gameLocal.IsTeamGame() && other->team == self->team ) {
		return false;
	}

	return true;
}

/*
================
rvBot::CanSee
================
*/
bool rvBot::CanSee( idPlayer *self, idEntity *other ) const {
	trace_t	trace;
	idVec3	eye;
	idMat3	axis;

	self->GetViewPos( eye, axis );

	const idVec3 target = other->GetPhysics()->GetAbsBounds().GetCenter();

	gameLocal.TracePoint( self, trace, eye, target, MASK_SHOT_BOUNDINGBOX, self );

	return ( trace.fraction >= 1.0f || trace.c.entityNum == other->entityNumber );
}

/*
================
rvBot::UpdateEnemy
================
*/
void rvBot::UpdateEnemy( idPlayer *self ) {
	idPlayer *	best		= NULL;
	float		bestDistSqr	= skill.sightRange * skill.sightRange;

	for ( int i = 0; i < gameLocal.numClients; i++ ) {
		idEntity *ent = gameLocal.entities[i];

		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}

		idPlayer *other = static_cast<idPlayer *>( ent );
		if ( !IsEnemy( self, other ) ) {
			continue;
		}

		const float distSqr = ( other->GetPhysics()->GetOrigin() - self->GetPhysics()->GetOrigin() ).LengthSqr();
		if ( distSqr >= bestDistSqr ) {
			continue;
		}
		if ( !CanSee( self, other ) ) {
			continue;
		}

		bestDistSqr	= distSqr;
		best		= other;
	}

	if ( best ) {
		// Restart the reaction delay on a genuinely new sighting: a different
		// player, or the same one stepping back into view.  Only checking the
		// identity would let a bot snap onto someone reappearing from cover.
		if ( enemy.GetEntity() != best || gameLocal.time - enemyLastSeenTime > BOT_REACQUIRE_MSEC ) {
			enemy				= best;
			enemyAcquiredTime	= gameLocal.time;
		}
		enemyLastSeenTime	= gameLocal.time;
		enemyLastSeenOrigin	= best->GetPhysics()->GetOrigin();
		return;
	}

	// Keep chasing a lost enemy for a moment, then forget it.
	idPlayer *current = enemy.GetEntity();
	if ( current && ( !IsEnemy( self, current ) || gameLocal.time - enemyLastSeenTime > BOT_ENEMY_MEMORY_MSEC ) ) {
		enemy = NULL;
	}
}

/*
================
rvBot::Repath
================
*/
bool rvBot::Repath( idPlayer *self, const idVec3 &goal ) {
	pathTime	= gameLocal.time;
	pathCorner	= 0;

	if ( !navMesh.FindPath( self->GetPhysics()->GetOrigin(), goal, path ) ) {
		path.Clear();
		repathFailures++;

		if ( bot_debug.GetInteger() >= 1 ) {
			gameLocal.Printf( "bot %s: no route from %s to %s (%d in a row)\n",
							  name.c_str(),
							  self->GetPhysics()->GetOrigin().ToString( 0 ),
							  goal.ToString( 0 ),
							  repathFailures );
		}
		return false;
	}

	repathFailures = 0;

	return true;
}

/*
================
rvBot::AbandonGoal

Drops the current goal and refuses to take it again for a while.  Without this
a bot that has parked on a weapon it cannot use will keep routing to the item
it is already standing on, forever.
================
*/
void rvBot::AbandonGoal( int forMsec ) {
	Avoid( goalEntity.GetEntity(), forMsec );

	goalType		= BOTGOAL_ROAM;
	goalEntity		= NULL;
	goalGiveUpTime	= 0;
	pathCorner		= 0;
	pathTime		= 0;
	path.Clear();
}

/*
================
rvBot::IsAvoided
================
*/
bool rvBot::IsAvoided( const idEntity *ent ) const {
	if ( !ent ) {
		return false;
	}

	for ( int i = 0; i < MAX_AVOID_GOALS; i++ ) {
		if ( avoidGoals[i].ent.GetEntity() == ent && gameLocal.time < avoidGoals[i].until ) {
			return true;
		}
	}

	return false;
}

/*
================
rvBot::Avoid
================
*/
void rvBot::Avoid( idEntity *ent, int forMsec ) {
	if ( !ent ) {
		return;
	}

	// Refresh an entry this entity already owns before taking a new slot.
	for ( int i = 0; i < MAX_AVOID_GOALS; i++ ) {
		if ( avoidGoals[i].ent.GetEntity() == ent ) {
			avoidGoals[i].until = gameLocal.time + forMsec;
			return;
		}
	}

	for ( int i = 0; i < MAX_AVOID_GOALS; i++ ) {
		if ( gameLocal.time >= avoidGoals[i].until ) {
			avoidGoals[i].ent	= ent;
			avoidGoals[i].until	= gameLocal.time + forMsec;
			return;
		}
	}

	// All still live: recycle round robin.
	avoidGoals[avoidNext].ent	= ent;
	avoidGoals[avoidNext].until	= gameLocal.time + forMsec;
	avoidNext = ( avoidNext + 1 ) % MAX_AVOID_GOALS;
}

/*
================
rvBot::RecoverToNavMesh

Called when there is no route at all.  Usually the bot has ended up somewhere
the mesh does not cover - blown off a ledge, telefragged into a corner - so it
heads for the closest thing it does know about, and failing that it just keeps
moving rather than standing still until the match ends.
================
*/
bool rvBot::RecoverToNavMesh( idPlayer *self, usercmd_t &cmd ) {
	const idVec3 origin = self->GetPhysics()->GetOrigin();

	// The search widens over a lot of empty grid when the bot really is off the
	// mesh, so it runs on a timer and the answer is reused in between.
	if ( gameLocal.time - recoverSearchTime > BOT_RECOVER_SEARCH_MSEC ) {
		recoverSearchTime = gameLocal.time;

		const int node = navMesh.FindNearestNode( origin, BOT_RECOVER_RADIUS );

		recoverValid = ( node != -1 );
		if ( recoverValid ) {
			recoverTarget = navMesh.GetNode( node ).origin;
		}
	}

	idVec3 moveDir;

	if ( recoverValid ) {
		moveDir = recoverTarget - origin;
		moveDir.z = 0.0f;

		// Only worth walking to if it is somewhere else.  A bot standing on the
		// mesh with no route needs to wander, not to shuffle on the spot.
		if ( moveDir.LengthSqr() > BOT_CORNER_REACH * BOT_CORNER_REACH && moveDir.Normalize() > 0.0f ) {
			ApplyMove( moveDir, cmd );
			PressJump( cmd );
			return true;
		}
	}

	// Nothing to head for: wander so the bot is at least a moving target.
	moveDir = aimAngles.ToForward();
	moveDir.z = 0.0f;

	if ( moveDir.Normalize() > 0.0f ) {
		ApplyMove( moveDir, cmd );
	}

	aimAngles.yaw = idMath::AngleNormalize180( aimAngles.yaw + 3.0f );

	return false;
}

/*
================
rvBot::PickItemGoal

Nearest item that is actually on the floor waiting to be taken and that the
navmesh says can be reached.  Item choice by value is a refinement; getting the
bot to move to things at all is the point here.
================
*/
idEntity *rvBot::PickItemGoal( idPlayer *self ) {
	idEntity *	best		= NULL;
	float		bestDistSqr	= BOT_ITEM_SEARCH_RANGE * BOT_ITEM_SEARCH_RANGE;
	idEntity *	ent;

	const idVec3 origin = self->GetPhysics()->GetOrigin();

	// Snap the bot onto the mesh once, not once per candidate: the reachability
	// test is a pair of ring searches and the bot's own end of it never changes
	// during the scan.
	const int startNode = navMesh.FindNearestNode( origin );
	const int startArea = ( startNode != -1 ) ? navMesh.GetNode( startNode ).area : -1;

	for ( ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		if ( !ent->IsType( idItem::GetClassType() ) ) {
			continue;
		}

		idItem *item = static_cast<idItem *>( ent );
		if ( item->pickedUp || item->IsHidden() ) {
			continue;
		}
		if ( IsAvoided( item ) ) {
			continue;
		}

		const float distSqr = ( item->GetPhysics()->GetOrigin() - origin ).LengthSqr();
		if ( distSqr >= bestDistSqr ) {
			continue;
		}
		const int itemNode = navMesh.FindNearestNode( item->GetPhysics()->GetOrigin() );
		if ( itemNode == -1 ) {
			continue;
		}
		if ( startArea != -1 && navMesh.GetNode( itemNode ).area != startArea ) {
			continue;
		}

		bestDistSqr	= distSqr;
		best		= item;
	}

	return best;
}

/*
================
rvBot::UpdateGoal
================
*/
void rvBot::UpdateGoal( idPlayer *self ) {
	const idVec3	origin		= self->GetPhysics()->GetOrigin();
	idPlayer *		currentFoe	= enemy.GetEntity();

	// A goal that has not been resolved in this long is not going to be.
	if ( goalGiveUpTime && gameLocal.time > goalGiveUpTime ) {
		if ( bot_debug.GetInteger() >= 1 ) {
			gameLocal.Printf( "bot %s: giving up on goal %d at %s\n", name.c_str(), goalType, goalOrigin.ToString( 0 ) );
		}
		AbandonGoal( BOT_GOAL_AVOID_MSEC );
	}

	// A live enemy usually wins: closing on it is what makes a bot read as a
	// player rather than a patrolling turret.
	//
	// The throttle counts *attempts*, not successes.  Keying it off goalType
	// instead would leave it dead on exactly the path it exists for: a failed
	// route never sets goalType to BOTGOAL_ENEMY, so the next frame would try
	// again, and a failed search is the expensive one - it drains the whole
	// forward-reachable set before returning false.
	if ( currentFoe ) {
		if ( gameLocal.time - enemyPathTime <= BOT_REPATH_COMBAT_MSEC ) {
			if ( goalType == BOTGOAL_ENEMY ) {
				// Already following a good route to them.
				return;
			}
			// The last attempt failed and it is too soon to try again; fall
			// through to whatever else there is to do.
		} else {
			enemyPathTime = gameLocal.time;

			if ( Repath( self, enemyLastSeenOrigin ) ) {
				goalType		= BOTGOAL_ENEMY;
				goalEntity		= currentFoe;
				goalOrigin		= enemyLastSeenOrigin;
				goalGiveUpTime	= gameLocal.time + BOT_GOAL_GIVEUP_MSEC;
				return;
			}

			// Cannot get to them.  Aiming and firing are handled elsewhere and
			// do not need a route, so fall through and find something else.
		}
	}

	// Goal still valid and route still fresh: leave it alone.
	if ( goalType == BOTGOAL_ITEM ) {
		idEntity *goal = goalEntity.GetEntity();
		idItem *item = ( goal && goal->IsType( idItem::GetClassType() ) ) ? static_cast<idItem *>( goal ) : NULL;

		if ( item && !item->pickedUp && !item->IsHidden() && !path.IsEmpty() ) {
			if ( gameLocal.time - pathTime <= BOT_REPATH_IDLE_MSEC ) {
				return;
			}
			goalOrigin = item->GetPhysics()->GetOrigin();
			Repath( self, goalOrigin );
			return;
		}
	}

	if ( goalType == BOTGOAL_ROAM && !path.IsEmpty() && pathCorner < path.Num() ) {
		if ( gameLocal.time - pathTime <= BOT_REPATH_IDLE_MSEC ) {
			return;
		}
	}

	// Picking a fresh goal walks every spawned entity and routes to the winner,
	// so it is throttled even when the previous attempt came up empty.  Without
	// this a bot with nothing reachable to do pays for the whole search every
	// frame.
	if ( gameLocal.time < nextGoalSelectTime ) {
		return;
	}
	nextGoalSelectTime = gameLocal.time + BOT_GOAL_SELECT_MSEC;

	// The give-up deadline is only armed when the goal actually changes.
	// Re-selecting the same goal every frame would otherwise keep pushing the
	// deadline out and the bot would never notice it was getting nowhere.
	idEntity *item = PickItemGoal( self );
	if ( item ) {
		if ( goalType != BOTGOAL_ITEM || goalEntity.GetEntity() != item ) {
			goalGiveUpTime = gameLocal.time + BOT_GOAL_GIVEUP_MSEC;
		}

		goalType	= BOTGOAL_ITEM;
		goalEntity	= item;
		goalOrigin	= item->GetPhysics()->GetOrigin();

		if ( Repath( self, goalOrigin ) ) {
			return;
		}
	}

	// Nothing worth having, or nothing reachable: wander.
	idVec3 roam;
	if ( navMesh.RandomReachablePoint( origin, roam ) ) {
		if ( goalType != BOTGOAL_ROAM ) {
			goalGiveUpTime = gameLocal.time + BOT_GOAL_GIVEUP_MSEC;
		}

		goalType	= BOTGOAL_ROAM;
		goalEntity	= NULL;
		goalOrigin	= roam;
		Repath( self, roam );
	}
}

/*
================
rvBot::AdvancePath

Returns false once the route is exhausted.
================
*/
bool rvBot::AdvancePath( idPlayer *self ) {
	const idVec3 origin = self->GetPhysics()->GetOrigin();

	while ( pathCorner < path.Num() ) {
		idVec3 delta = path[pathCorner].origin - origin;
		const float height = delta.z;
		delta.z = 0.0f;

		if ( delta.LengthSqr() > BOT_CORNER_REACH * BOT_CORNER_REACH ||
			 idMath::Fabs( height ) > BOT_CORNER_REACH_Z ) {
			return true;
		}

		pathCorner++;
	}

	return false;
}

/*
================
rvBot::ApplyMove

Movement is expressed in world space and resolved against the view, exactly as
a player's keys are.
================
*/
void rvBot::PressJump( usercmd_t &cmd ) const {
	// idPhysics_Player::CheckJump only fires on a fresh press: PMF_JUMP_HELD
	// blocks a held button and is only cleared on a frame where upmove is
	// released, and the press only takes if the bot happens to be on the ground
	// that frame.  So while a jump is wanted the button is cycled rather than
	// held or tapped once, fast enough that a press lands soon after the bot is
	// back on its feet.  The per-client offset stops a pack of bots hopping in
	// lockstep.
	const int phase = ( ( gameLocal.time + clientNum * BOT_JUMP_PHASE_MSEC / 2 ) / BOT_JUMP_PHASE_MSEC ) & 1;

	cmd.upmove = phase ? 127 : 0;
}

/*
================
rvBot::ApplyMove

Movement is expressed in world space and resolved against the view, exactly as
a player's keys are.
================
*/
void rvBot::ApplyMove( const idVec3 &moveDir, usercmd_t &cmd ) const {
	idVec3 forward, right;

	idAngles moveAngles( 0.0f, aimAngles.yaw, 0.0f );
	moveAngles.ToVectors( &forward, &right, NULL );

	cmd.forwardmove	= idMath::ClampChar( (int)( ( moveDir * forward ) * 127.0f ) );
	cmd.rightmove	= idMath::ClampChar( (int)( ( moveDir * right ) * 127.0f ) );

	// Full speed needs the run button held; without it idPlayer::AdjustSpeed
	// drops the bot to pm_walkspeed.
	if ( cmd.forwardmove || cmd.rightmove ) {
		cmd.buttons |= BUTTON_RUN;
	}
}

/*
================
rvBot::UpdateMovement
================
*/
void rvBot::UpdateMovement( idPlayer *self, usercmd_t &cmd ) {
	const idVec3 origin = self->GetPhysics()->GetOrigin();

	if ( !AdvancePath( self ) ) {
		// Arrived, or never had a route.  UpdateGoal picks a new one next frame,
		// unless routing itself keeps failing - then the bot is off the mesh and
		// has to walk its own way back on.
		path.Clear();
		pathCorner	= 0;
		pathTime	= 0;

		// Standing on an item that is still sitting there means it is not one
		// this bot can take - its own CTF flag, or a weapon it already has with
		// full ammo.  Routing to it again would loop forever.
		if ( goalType == BOTGOAL_ITEM ) {
			idEntity *goal = goalEntity.GetEntity();

			idItem *item = ( goal && goal->IsType( idItem::GetClassType() ) ) ? static_cast<idItem *>( goal ) : NULL;
			const bool stillThere = item && !item->pickedUp && !item->IsHidden();

			if ( stillThere && ( goal->GetPhysics()->GetOrigin() - origin ).LengthSqr() < BOT_ITEM_ARRIVED * BOT_ITEM_ARRIVED ) {
				if ( bot_debug.GetInteger() >= 1 ) {
					gameLocal.Printf( "bot %s: '%s' did not pick up, looking elsewhere\n",
									  name.c_str(), goal->GetClassname() );
				}
				AbandonGoal( BOT_GOAL_AVOID_MSEC );
			}
		}

		if ( repathFailures >= 2 || gameLocal.time - pathTime > BOT_RECOVER_IDLE_MSEC ) {
			RecoverToNavMesh( self, cmd );
		}
		return;
	}

	const navCorner_t &corner = path[pathCorner];

	idVec3 moveDir = corner.origin - origin;
	const float climb = moveDir.z;
	moveDir.z = 0.0f;

	if ( moveDir.Normalize() <= 0.0f ) {
		return;
	}

	// Sidestep while fighting so the bot is not a straight line target.
	if ( goalType == BOTGOAL_ENEMY && enemy.GetEntity() ) {
		const float range = ( enemy.GetEntity()->GetPhysics()->GetOrigin() - origin ).Length();

		if ( range < BOT_COMBAT_RANGE ) {
			idVec3 side = moveDir.Cross( idVec3( 0.0f, 0.0f, 1.0f ) );
			moveDir += side * ( unstickSide * skill.strafeChance );
			moveDir.Normalize();
		}
	}

	if ( gameLocal.time < unstickUntil ) {
		// Push sideways out of whatever the bot walked into.
		idVec3 side = moveDir.Cross( idVec3( 0.0f, 0.0f, 1.0f ) );
		moveDir += side * unstickSide;
		moveDir.Normalize();
	}

	ApplyMove( moveDir, cmd );

	// Jump when the route says to, when the next corner is above us, or to
	// shake off an obstruction.
	const bool wantJump = ( corner.travelType == NAVTRAVEL_JUMP ) ||
						  ( climb > pm_stepsize.GetFloat() ) ||
						  ( gameLocal.time < unstickUntil );

	if ( wantJump ) {
		PressJump( cmd );
	}

	// Stuck detection: no progress while trying to move means something the
	// navmesh does not know about is in the way.
	if ( gameLocal.time - stuckTime > BOT_STUCK_CHECK_MSEC ) {
		if ( ( origin - stuckOrigin ).LengthSqr() < BOT_STUCK_DISTANCE * BOT_STUCK_DISTANCE ) {
			unstickUntil	= gameLocal.time + BOT_UNSTICK_MSEC;
			unstickSide		= ( gameLocal.random.RandomFloat() < 0.5f ) ? -1.0f : 1.0f;

			// Two failures in a row means the route itself is bad.
			if ( path.Num() > 0 ) {
				path.Clear();
				pathTime = 0;
			}

			if ( bot_debug.GetBool() ) {
				gameLocal.Printf( "bot %s: stuck at %s\n", name.c_str(), origin.ToString( 0 ) );
			}
		}

		stuckOrigin	= origin;
		stuckTime	= gameLocal.time;
	}
}

/*
================
rvBot::UpdateAim
================
*/
void rvBot::UpdateAim( idPlayer *self, usercmd_t &cmd ) {
	idVec3	eye;
	idMat3	axis;
	idVec3	target;

	self->GetViewPos( eye, axis );

	idPlayer *foe = enemy.GetEntity();

	if ( foe ) {
		target = foe->GetPhysics()->GetAbsBounds().GetCenter();
	} else if ( pathCorner < path.Num() ) {
		target = path[pathCorner].origin;
		target.z += pm_normalheight.GetFloat() * 0.5f;
	} else {
		target = eye + aimAngles.ToForward() * 256.0f;
	}

	idVec3 dir = target - eye;
	if ( dir.Normalize() <= 0.0f ) {
		return;
	}

	// Re-roll the aim error now and then rather than every frame, so it reads
	// as a shaky hand instead of white noise.
	if ( gameLocal.time - aimOffsetTime > BOT_AIM_JITTER_MSEC ) {
		aimOffsetTime	= gameLocal.time;
		aimOffset.pitch	= gameLocal.random.CRandomFloat() * skill.aimError;
		aimOffset.yaw	= gameLocal.random.CRandomFloat() * skill.aimError;
		aimOffset.roll	= 0.0f;
	}

	idAngles desired = dir.ToAngles();
	desired.pitch	+= aimOffset.pitch;
	desired.yaw		+= aimOffset.yaw;
	desired.roll	= 0.0f;

	// Slew towards the target at the skill's turn rate.
	const float maxTurn = skill.turnSpeed * MS2SEC( gameLocal.GetMSec() );

	for ( int i = 0; i < 2; i++ ) {
		const float delta = idMath::AngleDelta( desired[i], aimAngles[i] );
		aimAngles[i] = idMath::AngleNormalize180( aimAngles[i] + idMath::ClampFloat( -maxTurn, maxTurn, delta ) );
	}

	aimAngles.pitch	= idMath::ClampFloat( -85.0f, 85.0f, aimAngles.pitch );
	aimAngles.roll	= 0.0f;

	// The player rebuilds its view as SHORT2ANGLE( cmd.angles ) + deltaViewAngles,
	// so the delta has to come back out here.
	for ( int i = 0; i < 3; i++ ) {
		cmd.angles[i] = ANGLE2SHORT( aimAngles[i] - self->GetDeltaViewAngles()[i] );
	}
}

/*
================
rvBot::UpdateWeapon
================
*/
void rvBot::UpdateWeapon( idPlayer *self ) {
	if ( gameLocal.time < nextWeaponTime ) {
		return;
	}
	nextWeaponTime = gameLocal.time + BOT_WEAPON_CHECK_MSEC;

	idPlayer *	foe		= enemy.GetEntity();
	float		range	= BOT_COMBAT_RANGE * 2.0f;

	if ( foe ) {
		range = ( foe->GetPhysics()->GetOrigin() - self->GetPhysics()->GetOrigin() ).Length();
	}

	const char **list = ( range < BOT_COMBAT_RANGE ) ? botWeaponsClose : botWeaponsFar;

	for ( int i = 0; list[i]; i++ ) {
		const int slot = self->GetWeaponIndex( list[i] );

		// GetWeaponIndex answers 0 for anything it does not know, so confirm
		// the slot really is the weapon that was asked for.
		if ( idStr::Icmp( self->spawnArgs.GetString( va( "def_weapon%d", slot ), "" ), list[i] ) != 0 ) {
			continue;
		}
		if ( !( self->inventory.weapons & ( 1 << slot ) ) ) {
			continue;
		}
		if ( !self->inventory.HasAmmo( list[i] ) ) {
			continue;
		}

		if ( slot != self->GetCurrentWeapon() ) {
			self->SelectWeapon( slot, false );
		}
		return;
	}
}

/*
================
rvBot::UpdateFire
================
*/
void rvBot::UpdateFire( idPlayer *self, usercmd_t &cmd ) {
	idPlayer *foe = enemy.GetEntity();

	if ( !foe ) {
		return;
	}
	if ( gameLocal.time - enemyAcquiredTime < skill.reactionMsec ) {
		return;
	}
	if ( gameLocal.time != enemyLastSeenTime ) {
		// Only shoot at what is visible right now.
		return;
	}

	idVec3	eye;
	idMat3	axis;

	self->GetViewPos( eye, axis );

	idVec3 dir = foe->GetPhysics()->GetAbsBounds().GetCenter() - eye;
	if ( dir.Normalize() <= 0.0f ) {
		return;
	}

	if ( dir * aimAngles.ToForward() < BOT_FIRE_CONE_DOT ) {
		return;
	}

	cmd.buttons |= BUTTON_ATTACK;
}

/*
================
rvBot::Think
================
*/
void rvBot::Think( usercmd_t &cmd ) {
	idPlayer *self = GetPlayer();

	// Start from a clean command every frame; anything not set below is
	// deliberately "not pressed".
	const int	previousAngles0 = cmd.angles[0];
	const int	previousAngles1 = cmd.angles[1];
	const int	previousAngles2 = cmd.angles[2];

	memset( &cmd, 0, sizeof( cmd ) );

	cmd.gameFrame	= gameLocal.framenum;
	cmd.gameTime	= gameLocal.time;
	cmd.angles[0]	= previousAngles0;
	cmd.angles[1]	= previousAngles1;
	cmd.angles[2]	= previousAngles2;

	if ( !self || bot_pause.GetBool() ) {
		return;
	}

	ResolveSkill();

	if ( self->health <= 0 || self->spectating ) {
		if ( self->health <= 0 ) {
			// idPlayer::EvaluateControls takes the attack button as a level, not
			// an edge, so holding it is enough to claim the respawn.
			cmd.buttons |= BUTTON_ATTACK;
		} else if ( gameLocal.time > nextRejoinTime ) {
			// Attack does not rejoin from spectate - it cycles the spectator's
			// view target.  Say the bot wants to play the way a human leaving
			// the spectate menu does, and let the game type decide when.
			nextRejoinTime = gameLocal.time + 1000;

			self->wantSpectate	= false;
			self->forceRespawn	= true;
		}

		if ( goalType != BOTGOAL_NONE ) {
			OnSpawn();
			goalType = BOTGOAL_NONE;
		}
		return;
	}

	if ( goalType == BOTGOAL_NONE ) {
		OnSpawn();
		goalType = BOTGOAL_ROAM;
	}

	UpdateEnemy( self );
	UpdateGoal( self );
	UpdateMovement( self, cmd );
	UpdateAim( self, cmd );
	UpdateWeapon( self );
	UpdateFire( self, cmd );

	if ( bot_debug.GetInteger() >= 2 && gameLocal.time >= nextStatusTime ) {
		static const char *goalNames[] = { "none", "roam", "item", "enemy" };

		nextStatusTime = gameLocal.time + 2000;

		idPlayer *foe = enemy.GetEntity();

		gameLocal.Printf( "bot %s: at %s hp %d goal %s corner %d/%d enemy %s%s\n",
						  name.c_str(),
						  self->GetPhysics()->GetOrigin().ToString( 0 ),
						  self->health,
						  goalNames[idMath::ClampInt( 0, 3, goalType )],
						  pathCorner, path.Num(),
						  foe ? gameLocal.userInfo[foe->entityNumber].GetString( "ui_name" ) : "-",
						  ( cmd.buttons & BUTTON_ATTACK ) ? " FIRING" : "" );
	}

	if ( bot_debugNav.GetInteger() >= 2 ) {
		for ( int i = idMath::ClampInt( 0, path.Num() - 1, pathCorner ); i < path.Num() - 1; i++ ) {
			gameRenderWorld->DebugArrow( colorRed, path[i].origin + idVec3( 0.0f, 0.0f, 8.0f ),
										 path[i + 1].origin + idVec3( 0.0f, 0.0f, 8.0f ), 4, 0 );
		}
	}
}

/*
===============================================================================

	rvBotManager

===============================================================================
*/

/*
================
rvBotManager::rvBotManager
================
*/
rvBotManager::rvBotManager( void ) {
	navBuilt			= false;
	navFailed			= false;
	nextMinPlayerCheck	= 0;
}

/*
================
rvBotManager::Init
================
*/
void rvBotManager::Init( void ) {
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		bots[i].Shutdown();
	}

	navBuilt			= false;
	navFailed			= false;
	nextMinPlayerCheck	= 0;
}

/*
================
rvBotManager::Shutdown
================
*/
void rvBotManager::Shutdown( void ) {
	Init();
	navMesh.Clear();
}

/*
================
rvBotManager::OnMapShutdown
================
*/
void rvBotManager::OnMapShutdown( void ) {
	navMesh.Clear();
	navBuilt	= false;
	navFailed	= false;

	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		bots[i].OnSpawn();
	}
}

/*
================
rvBotManager::IsBot
================
*/
bool rvBotManager::IsBot( int clientNum ) const {
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return false;
	}

	return bots[clientNum].IsActive();
}

/*
================
rvBotManager::NumBots
================
*/
int rvBotManager::NumBots( void ) const {
	int count = 0;

	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( bots[i].IsActive() ) {
			count++;
		}
	}

	return count;
}

/*
================
rvBotManager::FillUserInfo
================
*/
void rvBotManager::FillUserInfo( int clientNum, idDict &info ) const {
	if ( !IsBot( clientNum ) ) {
		return;
	}

	bots[clientNum].FillUserInfo( info );
}

/*
================
rvBotManager::EnsureNavMesh
================
*/
bool rvBotManager::EnsureNavMesh( void ) {
	if ( navBuilt && navMesh.IsValid() ) {
		return true;
	}
	if ( navFailed ) {
		return false;
	}

	navBuilt = true;

	if ( !navMesh.Build() ) {
		navFailed = true;
		return false;
	}

	return true;
}

/*
================
rvBotManager::PickBotName

First unused name from the table, so a full server does not end up with eight
players called the same thing and idGameLocal::SetUserInfo suffixing them all.
================
*/
const char *rvBotManager::PickBotName( void ) const {
	for ( int i = 0; botNames[i]; i++ ) {
		bool taken = false;

		for ( int j = 0; j < MAX_CLIENTS && !taken; j++ ) {
			if ( bots[j].IsActive() && idStr::Icmp( bots[j].GetName(), botNames[i] ) == 0 ) {
				taken = true;
			}
		}

		if ( !taken ) {
			return botNames[i];
		}
	}

	return botNames[0];
}

/*
================
rvBotManager::AddBot
================
*/
bool rvBotManager::AddBot( const char *name ) {
	if ( !gameLocal.isMultiplayer ) {
		gameLocal.Printf( "addbot: bots are multiplayer only\n" );
		return false;
	}
	if ( gameLocal.isClient || !gameLocal.isServer ) {
		gameLocal.Printf( "addbot: only the server can add bots\n" );
		return false;
	}
	if ( !bot_enable.GetBool() ) {
		gameLocal.Printf( "addbot: bots are disabled (bot_enable 0)\n" );
		return false;
	}

	if ( !EnsureNavMesh() ) {
		gameLocal.Printf( "addbot: no navigation could be generated for this map\n" );
		return false;
	}

	const char *botName = ( name && name[0] ) ? name : PickBotName();

	int maxPlayers = gameLocal.serverInfo.GetInt( "si_maxPlayers" );
	if ( maxPlayers <= 0 ) {
		maxPlayers = MAX_CLIENTS;
	}

	// The engine allocates the slot, then calls back into ServerClientBegin ->
	// SpawnPlayer, which is where OnSpawnPlayer registers the bot.
	const int clientNum = networkSystem->AllocateClientSlotForBot( botName, maxPlayers );

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		gameLocal.Printf( "addbot: no free client slot\n" );
		return false;
	}

	if ( !bots[clientNum].IsActive() ) {
		gameLocal.Warning( "addbot: client %d was allocated but never spawned a bot", clientNum );
		return false;
	}

	gameLocal.Printf( "addbot: '%s' joined as client %d\n", botName, clientNum );

	return true;
}

/*
================
rvBotManager::RemoveBot
================
*/
bool rvBotManager::RemoveBot( const char *name ) {
	int target = -1;

	for ( int i = MAX_CLIENTS - 1; i >= 0; i-- ) {
		if ( !bots[i].IsActive() ) {
			continue;
		}
		if ( name && name[0] && idStr::Icmp( bots[i].GetName(), name ) != 0 ) {
			continue;
		}

		target = i;
		break;
	}

	if ( target == -1 ) {
		return false;
	}

	// Drop through the server's own kick path so the slot, the userinfo and
	// every connected client are all cleaned up the usual way.
	cmdSystem->BufferCommandText( CMD_EXEC_APPEND, va( "kick %d\n", target ) );

	return true;
}

/*
================
rvBotManager::RemoveAll
================
*/
void rvBotManager::RemoveAll( void ) {
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( bots[i].IsActive() ) {
			cmdSystem->BufferCommandText( CMD_EXEC_APPEND, va( "kick %d\n", i ) );
		}
	}
}

/*
================
rvBotManager::ListBots
================
*/
void rvBotManager::ListBots( void ) const {
	int count = 0;

	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( !bots[i].IsActive() ) {
			continue;
		}

		gameLocal.Printf( "  %2d  %s\n", i, bots[i].GetName() );
		count++;
	}

	gameLocal.Printf( "%d bot%s, navmesh %s (%d nodes, %d links, built in %d ms)\n",
					  count, count == 1 ? "" : "s",
					  navMesh.IsValid() ? "ready" : "not built",
					  navMesh.NumNodes(), navMesh.NumLinks(), navMesh.GetBuildMilliseconds() );
}

/*
================
rvBotManager::OnSpawnPlayer
================
*/
void rvBotManager::OnSpawnPlayer( int clientNum, bool isBot, const char *botName ) {
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return;
	}

	if ( isBot ) {
		// A bot re-begun after a map change never goes through AddBot, so this
		// is the earliest point the new map's navmesh can be built.
		EnsureNavMesh();
		bots[clientNum].Init( clientNum, botName );
		return;
	}

	// A human took a slot a bot used to hold.
	if ( bots[clientNum].IsActive() ) {
		bots[clientNum].Shutdown();
	}
}

/*
================
rvBotManager::OnClientDisconnect
================
*/
void rvBotManager::OnClientDisconnect( int clientNum ) {
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return;
	}

	bots[clientNum].Shutdown();
}

/*
================
rvBotManager::CheckMinPlayers
================
*/
void rvBotManager::CheckMinPlayers( void ) {
	if ( gameLocal.time < nextMinPlayerCheck ) {
		return;
	}
	nextMinPlayerCheck = gameLocal.time + 2000;

	const int wanted = bot_minPlayers.GetInteger();
	if ( wanted <= 0 || !bot_enable.GetBool() ) {
		return;
	}

	int players = 0;
	int humans	= 0;

	for ( int i = 0; i < gameLocal.numClients; i++ ) {
		idEntity *ent = gameLocal.entities[i];
		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}

		players++;
		if ( !bots[i].IsActive() ) {
			humans++;
		}
	}

	if ( players < wanted ) {
		AddBot( NULL );
	} else if ( players > wanted && NumBots() > 0 && humans + NumBots() > wanted ) {
		RemoveBot( NULL );
	}
}

/*
================
rvBotManager::Think
================
*/
void rvBotManager::Think( void ) {
	if ( !gameLocal.isMultiplayer || gameLocal.isClient || !gameLocal.isServer ) {
		return;
	}

	CheckMinPlayers();

	if ( !gameLocal.usercmds ) {
		return;
	}

	// Bots are client slots and survive a map change; the navmesh describes the
	// map and does not.  Rebuild it here rather than only in AddBot, or bots
	// carried into the next map would stand still for the rest of it.
	if ( !navMesh.IsValid() && NumBots() > 0 ) {
		if ( !EnsureNavMesh() ) {
			return;
		}
	}

	// The engine hands the game a const view of its own user command array for
	// this frame; a bot's slot is ours to fill in, exactly where a remote
	// player's packet would have landed.
	usercmd_t *cmds = const_cast<usercmd_t *>( gameLocal.usercmds );

	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( bots[i].IsActive() ) {
			bots[i].Think( cmds[i] );
		}
	}

	if ( bot_debugNav.GetInteger() >= 1 && navMesh.IsValid() ) {
		idPlayer *viewer = gameLocal.GetLocalPlayer();
		if ( viewer ) {
			navMesh.DebugDraw( viewer->GetPhysics()->GetOrigin(), 1024.0f );
		}
	}
}
