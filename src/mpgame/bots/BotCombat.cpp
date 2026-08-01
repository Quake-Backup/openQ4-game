//----------------------------------------------------------------
// BotCombat.cpp
//----------------------------------------------------------------

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "../Projectile.h"
#include "BotCombat.h"

// Points remain slightly inside the collision bounds.  Chest first gives a
// stable centre-mass aim when the whole player is exposed; head and pelvis are
// fallbacks for partial cover.
static const float BOTCOMBAT_CHEST_FRACTION	= 0.62f;
static const float BOTCOMBAT_HEAD_FRACTION	= 0.84f;
static const float BOTCOMBAT_PELVIS_FRACTION	= 0.38f;
static const float BOTCOMBAT_LATERAL_INSET	= 0.32f;

// Combat queries run on every active bot, so trajectory work is deliberately
// bounded.  A 48-segment cap still gives a fast MP nail a segment shorter than
// a player hull over the caller's ordinary sub-second dodge horizon.
static const int   BOTCOMBAT_MIN_TRAJECTORY_STEPS = 6;
static const int   BOTCOMBAT_MAX_TRAJECTORY_STEPS = 48;
static const float BOTCOMBAT_MAX_SHOT_FLIGHT	= 2.0f;
static const float BOTCOMBAT_MIN_PROJECTILE_EXTENT = 0.25f;

struct botCombatShotModel_t {
	const idDict *projectileDef;
	float		 speed;
	idVec3		 gravity;
	idBounds	 bounds;
	float		 boundsRadius;
	int			 clipMask;

	botCombatShotModel_t() :
		projectileDef( NULL ),
		speed( 0.0f ),
		gravity( vec3_zero ),
		bounds( idVec3( -BOTCOMBAT_MIN_PROJECTILE_EXTENT, -BOTCOMBAT_MIN_PROJECTILE_EXTENT, -BOTCOMBAT_MIN_PROJECTILE_EXTENT ),
				idVec3( BOTCOMBAT_MIN_PROJECTILE_EXTENT, BOTCOMBAT_MIN_PROJECTILE_EXTENT, BOTCOMBAT_MIN_PROJECTILE_EXTENT ) ),
		boundsRadius( BOTCOMBAT_MIN_PROJECTILE_EXTENT ),
		clipMask( MASK_SHOT_BOUNDINGBOX ) {
	}
};

/*
================
BotCombatSymmetricProjectileBounds

Entity `size` bounds are based at z=0 and projectile trace models rotate in
flight.  TraceBounds has no axis parameter, so use the conservative symmetric
extents of the authored bounds.  The live-projectile query uses its physics
bounds through this same conversion.
================
*/
static idBounds BotCombatSymmetricProjectileBounds( const idBounds &authored ) {
	idVec3 extent;

	for ( int i = 0; i < 3; i++ ) {
		extent[i] = Max( BOTCOMBAT_MIN_PROJECTILE_EXTENT,
			Max( idMath::Fabs( authored[0][i] ), idMath::Fabs( authored[1][i] ) ) );
	}

	return idBounds( -extent, extent );
}

/*
================
BotCombatProjectileBoundsFromDef
================
*/
static idBounds BotCombatProjectileBoundsFromDef( const idDict *projectileDef ) {
	idBounds authored;
	idVec3 size;

	if ( projectileDef &&
		 projectileDef->GetVector( "mins", NULL, authored[0] ) &&
		 projectileDef->GetVector( "maxs", NULL, authored[1] ) ) {
		return BotCombatSymmetricProjectileBounds( authored );
	}

	if ( projectileDef && projectileDef->GetVector( "size", NULL, size ) ) {
		authored[0].Set( size.x * -0.5f, size.y * -0.5f, 0.0f );
		authored[1].Set( size.x * 0.5f, size.y * 0.5f, size.z );
		return BotCombatSymmetricProjectileBounds( authored );
	}

	return idBounds( idVec3( -BOTCOMBAT_MIN_PROJECTILE_EXTENT, -BOTCOMBAT_MIN_PROJECTILE_EXTENT, -BOTCOMBAT_MIN_PROJECTILE_EXTENT ),
					 idVec3( BOTCOMBAT_MIN_PROJECTILE_EXTENT, BOTCOMBAT_MIN_PROJECTILE_EXTENT, BOTCOMBAT_MIN_PROJECTILE_EXTENT ) );
}

/*
================
BotCombatProjectileClipMask

Match idProjectile::Launch's content policy closely enough for a speculative
shot sweep.  The live threat path uses the projectile's exact physics mask.
================
*/
static int BotCombatProjectileClipMask( const idDict *projectileDef ) {
	if ( !projectileDef ) {
		return MASK_SHOT_BOUNDINGBOX;
	}

	int mask = projectileDef->GetBool( "clipmask_rendermodel", "1" ) ?
		MASK_SHOT_RENDERMODEL : MASK_SHOT_BOUNDINGBOX;
	mask |= CONTENTS_PROJECTILECLIP | CONTENTS_PROJECTILE | CONTENTS_WATER;
	if ( projectileDef->GetBool( "clipmask_largeshot", "1" ) ) {
		mask |= CONTENTS_LARGESHOTCLIP;
	}
	if ( projectileDef->GetBool( "clipmask_moveable", "0" ) ) {
		mask |= CONTENTS_MOVEABLECLIP;
	}
	if ( projectileDef->GetBool( "clipmask_monsterclip", "0" ) ) {
		mask |= CONTENTS_MONSTERCLIP;
	}

	return mask;
}

/*
================
BotCombatCurrentShotModel

rvWeapon::spawnArgs is the mod-resolved view of the currently selected weapon;
FindEntityDefDict then applies the multiplayer projectile override.
================
*/
static bool BotCombatCurrentShotModel( idPlayer *shooter, botCombatShotModel_t &model ) {
	if ( !shooter || !shooter->weapon ) {
		return false;
	}

	const char *projectileName = shooter->weapon->spawnArgs.GetString( "def_projectile", "" );
	if ( !projectileName[0] ) {
		return false;
	}

	model.projectileDef = gameLocal.FindEntityDefDict( projectileName, false );
	if ( !model.projectileDef ) {
		return false;
	}

	model.speed = idProjectile::GetVelocity( model.projectileDef ).x;
	if ( model.speed <= idMath::FLOAT_EPSILON ) {
		return false;
	}

	model.gravity		= idProjectile::GetGravity( model.projectileDef );
	model.bounds		= BotCombatProjectileBoundsFromDef( model.projectileDef );
	model.boundsRadius	= model.bounds.GetRadius();
	model.clipMask		= BotCombatProjectileClipMask( model.projectileDef );
	return true;
}

/*
================
BotCombatTrajectoryPoint
================
*/
static idVec3 BotCombatTrajectoryPoint( const idVec3 &origin, const idVec3 &velocity,
									 const idVec3 &gravity, float time ) {
	return origin + velocity * time + gravity * ( 0.5f * time * time );
}

/*
================
BotCombatProjectileSplashRadius
================
*/
static float BotCombatProjectileSplashRadius( const idProjectile *projectile ) {
	if ( !projectile ) {
		return 0.0f;
	}

	const char *damageName = projectile->spawnArgs.GetString( "def_splash_damage", "" );
	const idDict *damageDef = damageName[0] ? gameLocal.FindEntityDefDict( damageName, false ) : NULL;
	return damageDef ? Max( 0.0f, damageDef->GetFloat( "radius", "0" ) ) : 0.0f;
}

/*
================
BotCombatTraceEntity

GetTraceEntity resolves a hit on a bound player attachment back to its master,
which is important for models whose head owns a separate clip model.
================
*/
static idEntity *BotCombatTraceEntity( const trace_t &trace ) {
	if ( trace.c.entityNum < 0 || trace.c.entityNum >= MAX_GENTITIES ) {
		return NULL;
	}

	return gameLocal.GetTraceEntity( trace );
}

/*
================
BotCombatSameTeam
================
*/
static bool BotCombatSameTeam( const idPlayer *self, const idPlayer *other ) {
	return self && other && gameLocal.IsTeamGame() && self->team == other->team;
}

/*
================
BotCombatValidClientPlayer

CanPlay indexes multiplayer state by entityNumber, so every helper that accepts
a player from an entity pointer must establish the client range first.
================
*/
static bool BotCombatValidClientPlayer( idPlayer *player ) {
	return player && player->entityNumber >= 0 && player->entityNumber < MAX_CLIENTS &&
		player->GetPhysics();
}

/*
================
BotCombatValidFoe

The target can die, spectate, change team, or move to another arena after the
bot selected it and before the final trigger decision.
================
*/
static bool BotCombatValidFoe( idPlayer *shooter, idPlayer *foe ) {
	if ( !BotCombatValidClientPlayer( shooter ) || !BotCombatValidClientPlayer( foe ) ||
		 !gameLocal.mpGame.CanPlay( shooter ) || shooter->spectating || shooter->health <= 0 ||
		 foe == shooter || foe->GetInstance() != shooter->GetInstance() ||
		 !gameLocal.mpGame.CanPlay( foe ) || foe->spectating || foe->health <= 0 ) {
		return false;
	}

	return !BotCombatSameTeam( shooter, foe );
}

/*
================
BotCombatPredictedBoundsExposed

CanDamage is ideal at time zero because it samples the live entity hull.  For a
projectile arriving later, trace to the same centre/corner pattern translated
by current velocity.  World cover still counts, while the teammate's present
clip model is not mistaken for their future position.
================
*/
static bool BotCombatPredictedBoundsExposed( idPlayer *player, idEntity *splashIgnore,
											  const idVec3 &impact, float impactTime ) {
	if ( impactTime <= 0.05f ) {
		idVec3 damagePoint;
		return player->CanDamage( impact, damagePoint, splashIgnore );
	}

	idBounds predicted = player->GetPhysics()->GetAbsBounds() +
		player->GetPhysics()->GetLinearVelocity() * impactTime;
	const idVec3 middle = predicted.GetCenter();
	const float insetX = Min( 15.0f, ( predicted[1].x - predicted[0].x ) * 0.35f );
	const float insetY = Min( 15.0f, ( predicted[1].y - predicted[0].y ) * 0.35f );
	const float insetZ = Min( 15.0f, ( predicted[1].z - predicted[0].z ) * 0.25f );
	const idVec3 samples[] = {
		middle,
		middle + idVec3( insetX,  insetY, 0.0f ),
		middle + idVec3( insetX, -insetY, 0.0f ),
		middle + idVec3( -insetX, insetY, 0.0f ),
		middle + idVec3( -insetX, -insetY, 0.0f ),
		middle + idVec3( 0.0f, 0.0f, insetZ ),
		middle - idVec3( 0.0f, 0.0f, insetZ )
	};

	for ( int i = 0; i < (int)( sizeof( samples ) / sizeof( samples[0] ) ); i++ ) {
		trace_t trace;
		gameLocal.TracePoint( player, trace, impact, samples[i], MASK_SOLID, splashIgnore );
		if ( trace.fraction >= 1.0f || BotCombatTraceEntity( trace ) == player ) {
			return true;
		}
	}

	return false;
}

/*
================
BotCombatSplashThreatensPlayer
================
*/
static bool BotCombatSplashThreatensPlayer( idPlayer *player, idEntity *splashIgnore,
											 const idVec3 &impact, float radius,
											 float impactTime ) {
	if ( !BotCombatValidClientPlayer( player ) || radius <= 0.0f ) {
		return false;
	}

	const idBounds predicted = player->GetPhysics()->GetAbsBounds() +
		player->GetPhysics()->GetLinearVelocity() * Max( 0.0f, impactTime );
	if ( predicted.ShortestDistance( impact ) >= radius ) {
		return false;
	}

	return BotCombatPredictedBoundsExposed( player, splashIgnore, impact,
		Max( 0.0f, impactTime ) );
}

/*
================
BotCombatSplashThreatensTeamMate

Direct traces only protect a teammate standing in the projectile path.  Splash
also reaches players beside the impact, so mirror RadiusDamage's bounds-distance
and exposure tests for live teammates in this arena, projected to impact time.
================
*/
static bool BotCombatSplashThreatensTeamMate( idPlayer *shooter, idEntity *splashIgnore,
											  const idVec3 &impact, float radius,
											  float impactTime ) {
	if ( !shooter || !gameLocal.IsTeamGame() || radius <= 0.0f ) {
		return false;
	}

	for ( int i = 0; i < gameLocal.numClients; i++ ) {
		idPlayer *teamMate = gameLocal.GetClientByNum( i );
		if ( !BotCombatValidClientPlayer( teamMate ) || teamMate == shooter ||
			 teamMate->GetInstance() != shooter->GetInstance() ||
			 !gameLocal.mpGame.CanPlay( teamMate ) || teamMate->spectating ||
			 teamMate->health <= 0 || !BotCombatSameTeam( shooter, teamMate ) ) {
			continue;
		}

		if ( BotCombatSplashThreatensPlayer( teamMate, splashIgnore, impact,
											 radius, impactTime ) ) {
			return true;
		}
	}

	return false;
}

/*
================
BotCombatShotCrossesMovingTeamMate

A point trace only sees where a teammate is now.  Transform each projectile
segment into the teammate's moving frame and intersect it with an expanded
copy of their current hull.  This catches the classic rocket/team-mate doorway
cross even when neither occupies the intersection point on the firing frame.
================
*/
static bool BotCombatShotCrossesMovingTeamMate( idPlayer *shooter,
												 const idVec3 &segmentStart,
												 const idVec3 &segmentEnd,
												 float startTime, float endTime,
												 float projectileRadius ) {
	if ( !shooter || !gameLocal.IsTeamGame() ) {
		return false;
	}

	for ( int i = 0; i < gameLocal.numClients; i++ ) {
		idPlayer *teamMate = gameLocal.GetClientByNum( i );
		if ( !BotCombatValidClientPlayer( teamMate ) || teamMate == shooter ||
			 teamMate->GetInstance() != shooter->GetInstance() ||
			 !gameLocal.mpGame.CanPlay( teamMate ) || teamMate->spectating ||
			 teamMate->health <= 0 || !BotCombatSameTeam( shooter, teamMate ) ) {
			continue;
		}

		const idVec3 velocity = teamMate->GetPhysics()->GetLinearVelocity();
		const idVec3 relativeStart = segmentStart - velocity * Max( 0.0f, startTime );
		const idVec3 relativeEnd = segmentEnd - velocity * Max( 0.0f, endTime );
		const idBounds corridor = teamMate->GetPhysics()->GetAbsBounds().Expand(
			Max( 0.0f, projectileRadius ) );

		if ( corridor.LineIntersection( relativeStart, relativeEnd ) ) {
			return true;
		}
	}

	return false;
}

/*
================
BotCombatHostileProjectileOwner

The projectile's own instance decides whether it can reach this bot.  A valid,
same-instance owner can prove the projectile friendly; a missing or stale owner
cannot prove that old ordnance is safe after a disconnect, spectate, or arena
transition.
================
*/
static bool BotCombatHostileProjectileOwner( idPlayer *self, idPlayer *owner ) {
	if ( !BotCombatValidClientPlayer( self ) ) {
		return false;
	}
	if ( !BotCombatValidClientPlayer( owner ) ||
		 owner->GetInstance() != self->GetInstance() ||
		 !gameLocal.mpGame.CanPlay( owner ) ) {
		return true;
	}
	if ( owner == self || BotCombatSameTeam( self, owner ) ) {
		return false;
	}

	return true;
}

/*
================
BotCombatProjectileCanDamage

Unknown-owner projectiles are treated conservatively only when their live
definition contains a direct, splash, residual, or emitted damage channel.
This keeps cosmetic and scripted non-damaging projectiles out of dodge scans.
================
*/
static bool BotCombatProjectileCanDamage( const idProjectile *projectile ) {
	if ( !projectile ) {
		return false;
	}

	static const char *damageKeys[] = {
		"def_damage",
		"def_splash_damage",
		"def_residual_damage",
		"def_emit_damage"
	};
	for ( int i = 0; i < (int)( sizeof( damageKeys ) / sizeof( damageKeys[0] ) ); i++ ) {
		if ( projectile->spawnArgs.GetString( damageKeys[i], "" )[0] ) {
			return true;
		}
	}

	return false;
}

/*
================
BotCombatFindVisibleAimPoint
================
*/
bool BotCombatFindVisibleAimPoint( idPlayer *observer, idPlayer *target,
								   idVec3 &visiblePoint ) {
	visiblePoint.Zero();

	if ( !BotCombatValidClientPlayer( observer ) || !BotCombatValidClientPlayer( target ) ||
		 observer == target ) {
		return false;
	}
	if ( observer->GetInstance() != target->GetInstance() ) {
		return false;
	}
	if ( observer->spectating || observer->health <= 0 || !gameLocal.mpGame.CanPlay( observer ) ||
		 target->spectating || target->health <= 0 || !gameLocal.mpGame.CanPlay( target ) ) {
		return false;
	}

	idVec3 eye;
	idMat3 axis;

	observer->GetViewPos( eye, axis );

	const idBounds &bounds = target->GetPhysics()->GetAbsBounds();
	const float height = bounds[1].z - bounds[0].z;

	if ( height <= idMath::FLOAT_EPSILON ) {
		return false;
	}

	idVec3 targetEye;
	idMat3 targetAxis;
	target->GetViewPos( targetEye, targetAxis );
	(void)targetEye;
	idVec3 lateral = targetAxis[1];
	lateral.z = 0.0f;
	if ( lateral.Normalize() <= idMath::FLOAT_EPSILON ) {
		lateral.Set( 0.0f, 1.0f, 0.0f );
	}
	const float lateralDistance = Min( bounds[1].x - bounds[0].x,
		bounds[1].y - bounds[0].y ) * BOTCOMBAT_LATERAL_INSET;
	lateral *= lateralDistance;

	idVec3 chest = bounds.GetCenter();
	chest.z = bounds[0].z + height * BOTCOMBAT_CHEST_FRACTION;
	idVec3 head = bounds.GetCenter();
	head.z = bounds[0].z + height * BOTCOMBAT_HEAD_FRACTION;
	idVec3 pelvis = bounds.GetCenter();
	pelvis.z = bounds[0].z + height * BOTCOMBAT_PELVIS_FRACTION;
	idVec3 samples[] = {
		chest,
		head,
		pelvis,
		chest + lateral,
		chest - lateral,
		pelvis + lateral,
		pelvis - lateral
	};

	for ( int i = 0; i < (int)( sizeof( samples ) / sizeof( samples[0] ) ); i++ ) {
		idVec3 sample = samples[i];
		// Diagonal facing axes can put a shoulder sample just outside one side of
		// an axis-aligned world bound.  Keep every fallback inside the live hull.
		sample.x = idMath::ClampFloat( bounds[0].x + 0.5f, bounds[1].x - 0.5f, sample.x );
		sample.y = idMath::ClampFloat( bounds[0].y + 0.5f, bounds[1].y - 0.5f, sample.y );

		trace_t trace;
		gameLocal.TracePoint( observer, trace, eye, sample, MASK_SHOT_BOUNDINGBOX, observer );

		if ( trace.fraction >= 1.0f || BotCombatTraceEntity( trace ) == target ) {
			visiblePoint = sample;
			return true;
		}
	}

	return false;
}

/*
================
BotCombatLineOfFireIsSafe
================
*/
bool BotCombatLineOfFireIsSafe( idPlayer *shooter, idPlayer *intendedFoe,
								const idVec3 &shotOrigin, const idVec3 &aimPoint,
								float splashSafetyRadius, bool requireUsefulImpact,
								idVec3 *impactPoint ) {
	if ( impactPoint ) {
		*impactPoint = shotOrigin;
	}
	if ( !BotCombatValidFoe( shooter, intendedFoe ) ||
		 ( aimPoint - shotOrigin ).LengthSqr() <= idMath::FLOAT_EPSILON ) {
		return false;
	}

	idVec3 shotDirection = aimPoint - shotOrigin;
	const float aimDistance = shotDirection.Normalize();

	trace_t trace;
	trace.fraction = 1.0f;
	trace.endpos = aimPoint;
	idVec3 actualImpact = aimPoint;
	float impactTime = 0.0f;
	idEntity *hit = NULL;

	botCombatShotModel_t shotModel;
	if ( BotCombatCurrentShotModel( shooter, shotModel ) ) {
		const float minimumFlight = Max( MS2SEC( gameLocal.GetMSec() ), 0.01f );
		const float flightTime = idMath::ClampFloat( minimumFlight,
			BOTCOMBAT_MAX_SHOT_FLIGHT, aimDistance / shotModel.speed );
		const idVec3 launchVelocity = shotDirection * shotModel.speed;
		const float estimatedTravel = ( shotModel.speed +
			shotModel.gravity.Length() * flightTime * 0.5f ) * flightTime;
		const int steps = idMath::ClampInt( BOTCOMBAT_MIN_TRAJECTORY_STEPS,
			BOTCOMBAT_MAX_TRAJECTORY_STEPS, idMath::Ceil( estimatedTravel / 48.0f ) );

		idVec3 previous = shotOrigin;
		float previousTime = 0.0f;
		for ( int i = 1; i <= steps; i++ ) {
			const float segmentTime = flightTime * ( (float)i / (float)steps );
			const idVec3 segmentEnd = BotCombatTrajectoryPoint( shotOrigin, launchVelocity,
				shotModel.gravity, segmentTime );

			gameLocal.TraceBounds( shooter, trace, previous, segmentEnd, shotModel.bounds,
				shotModel.clipMask, shooter );
			const float tracedTime = previousTime +
				( segmentTime - previousTime ) * idMath::ClampFloat( 0.0f, 1.0f, trace.fraction );

			if ( BotCombatShotCrossesMovingTeamMate( shooter, previous, trace.endpos,
												previousTime, tracedTime,
												shotModel.boundsRadius ) ) {
				return false;
			}

			actualImpact = trace.endpos;
			impactTime = tracedTime;
			if ( trace.fraction < 1.0f ) {
				hit = BotCombatTraceEntity( trace );
				break;
			}

			previous = segmentEnd;
			previousTime = segmentTime;
		}
	} else {
		gameLocal.TracePoint( shooter, trace, shotOrigin, aimPoint,
							 MASK_SHOT_BOUNDINGBOX, shooter );
		actualImpact = ( trace.fraction < 1.0f ) ? trace.endpos : aimPoint;
		hit = ( trace.fraction < 1.0f ) ? BotCombatTraceEntity( trace ) : NULL;
	}

	if ( impactPoint ) {
		*impactPoint = actualImpact;
	}

	idPlayer *hitPlayer = ( hit && hit->IsType( idPlayer::GetClassType() ) ) ?
		static_cast<idPlayer *>( hit ) : NULL;

	// Test team safety before accepting the intended target.  A bad caller must
	// not be able to authorize friendly fire by labelling a teammate as the foe.
	if ( hitPlayer && ( hitPlayer == shooter || BotCombatSameTeam( shooter, hitPlayer ) ) ) {
		return false;
	}

	// Predict where the shooter and teammates will be when the projectile gets
	// there.  Exposure tracing permits intentional cover-assisted splash shots.
	idEntity *splashIgnore = hit ? hit : intendedFoe;
	if ( BotCombatSplashThreatensPlayer( shooter, splashIgnore, actualImpact,
										 splashSafetyRadius, impactTime ) ) {
		return false;
	}
	if ( BotCombatSplashThreatensTeamMate( shooter, splashIgnore, actualImpact,
										 splashSafetyRadius, impactTime ) ) {
		return false;
	}

	if ( hitPlayer == intendedFoe ) {
		return true;
	}

	if ( requireUsefulImpact ) {
		// A projectile may correctly aim above or beside the player and terminate
		// at its feet.  Reuse the same impact-time bounds and exposure model used
		// by splash safety instead of reintroducing a straight eye-to-foe ray.
		idEntity *splashTargetIgnore = hit ? hit : shooter;
		return BotCombatSplashThreatensPlayer( intendedFoe, splashTargetIgnore,
			actualImpact, splashSafetyRadius, impactTime );
	}

	// Hitting another hostile player or a distant piece of world is safe for
	// deliberate suppression.  The caller explicitly opted out of usefulness.
	return true;
}

struct botCombatProjectileImpact_t {
	bool		 terminating;
	bool		 hitsSelf;
	float		 time;
	idVec3		 point;
	idEntity	*hit;

	botCombatProjectileImpact_t() :
		terminating( false ), hitsSelf( false ), time( 0.0f ),
		point( vec3_zero ), hit( NULL ) {
	}
};

/*
================
BotCombatProjectileImpactTerminates

Bounce projectiles are deliberately not extrapolated after their first bounce;
continuing the old parabola is safer than inventing a reflected direction.
Only a collision that the live definition says detonates can prove the original
path ended before reaching the bot.
================
*/
static bool BotCombatProjectileImpactTerminates( const idProjectile *projectile,
												   idEntity *hit ) {
	if ( !projectile ) {
		return false;
	}

	const bool actorHit = hit && hit->IsType( idActor::GetClassType() );
	return actorHit ? projectile->spawnArgs.GetBool( "detonate_on_actor", "0" ) :
		projectile->spawnArgs.GetBool( "detonate_on_world", "0" );
}

/*
================
BotCombatFindProjectileImpact

Sweep the live projectile hull over short parabolic segments.  This filters a
rocket safely intercepted by a wall while preserving a threat when that wall
impact is close enough for splash.  A non-terminating bounce makes future path
prediction uncertain and therefore cannot be used as proof of safety.
================
*/
static botCombatProjectileImpact_t BotCombatFindProjectileImpact(
		idProjectile *projectile, idPlayer *self, float horizon, int steps ) {
	botCombatProjectileImpact_t result;
	if ( !projectile || !projectile->GetPhysics() || horizon <= 0.0f || steps <= 0 ) {
		return result;
	}

	const idVec3 origin = projectile->GetPhysics()->GetOrigin();
	const idVec3 velocity = projectile->GetPhysics()->GetLinearVelocity();
	const idVec3 gravity = projectile->GetPhysics()->GetGravity();
	const idBounds bounds = BotCombatSymmetricProjectileBounds(
		projectile->GetPhysics()->GetBounds() );
	const int clipMask = projectile->GetPhysics()->GetClipMask();
	idVec3 previous = origin;
	float previousTime = 0.0f;

	for ( int i = 1; i <= steps; i++ ) {
		const float segmentTime = horizon * ( (float)i / (float)steps );
		const idVec3 segmentEnd = BotCombatTrajectoryPoint( origin, velocity, gravity,
			segmentTime );
		trace_t trace;
		gameLocal.TraceBounds( projectile, trace, previous, segmentEnd, bounds,
			clipMask, projectile );

		if ( trace.fraction < 1.0f ) {
			idEntity *hit = BotCombatTraceEntity( trace );
			result.hitsSelf = ( hit == self );
			result.terminating = result.hitsSelf ||
				BotCombatProjectileImpactTerminates( projectile, hit );
			result.time = previousTime + ( segmentTime - previousTime ) *
				idMath::ClampFloat( 0.0f, 1.0f, trace.fraction );
			result.point = trace.endpos;
			result.hit = hit;
			return result;
		}

		previous = segmentEnd;
		previousTime = segmentTime;
	}

	return result;
}

/*
================
BotCombatProjectileDistanceAtTime
================
*/
static float BotCombatProjectileDistanceAtTime( const idBounds &selfBounds,
												  const idVec3 &selfVelocity,
												  const idVec3 &projectileOrigin,
												  const idVec3 &projectileVelocity,
												  const idVec3 &projectileGravity,
												  float time, idVec3 *projectilePoint ) {
	const idVec3 point = BotCombatTrajectoryPoint( projectileOrigin, projectileVelocity,
		projectileGravity, time );
	if ( projectilePoint ) {
		*projectilePoint = point;
	}
	return ( selfBounds + selfVelocity * time ).ShortestDistance( point );
}

/*
================
BotCombatFindIncomingProjectileThreat
================
*/
bool BotCombatFindIncomingProjectileThreat( idPlayer *self,
									 float lookAheadSeconds, float threatClearance,
									 idProjectile *&threatProjectile,
									 float &timeToClosest, idVec3 &closestPoint ) {
	threatProjectile	= NULL;
	timeToClosest		= 0.0f;
	closestPoint.Zero();

	if ( !BotCombatValidClientPlayer( self ) || lookAheadSeconds <= 0.0f ) {
		return false;
	}

	const idBounds selfBounds = self->GetPhysics()->GetAbsBounds();
	const idVec3 selfVelocity = self->GetPhysics()->GetLinearVelocity();
	const float extraClearance = Max( 0.0f, threatClearance );

	float bestTime = idMath::INFINITY;
	float bestDistance = idMath::INFINITY;

	for ( idEntity *ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		if ( !ent->IsType( idProjectile::GetClassType() ) ) {
			continue;
		}

		idProjectile *projectile = static_cast<idProjectile *>( ent );
		if ( projectile->IsHidden() || projectile->GetInstance() != self->GetInstance() ) {
			continue;
		}
		if ( !BotCombatProjectileCanDamage( projectile ) ) {
			continue;
		}

		idEntity *ownerEntity = projectile->GetOwner();
		idPlayer *owner = ( ownerEntity && ownerEntity->IsType( idPlayer::GetClassType() ) ) ?
			static_cast<idPlayer *>( ownerEntity ) : NULL;

		if ( !BotCombatHostileProjectileOwner( self, owner ) ) {
			continue;
		}

		const idVec3 projectileOrigin = projectile->GetPhysics()->GetOrigin();
		const idVec3 projectileVelocity = projectile->GetPhysics()->GetLinearVelocity();
		const idVec3 projectileGravity = projectile->GetPhysics()->GetGravity();
		const float projectileRadius = projectile->GetPhysics()->GetBounds().GetRadius();
		const float unsafeClearance = extraClearance + projectileRadius;
		const float estimatedTravel = ( projectileVelocity.Length() +
			projectileGravity.Length() * lookAheadSeconds * 0.5f ) * lookAheadSeconds;
		const float segmentLength = Max( 16.0f, unsafeClearance * 0.35f );
		const int steps = idMath::ClampInt( BOTCOMBAT_MIN_TRAJECTORY_STEPS,
			BOTCOMBAT_MAX_TRAJECTORY_STEPS,
			(int)idMath::Ceil( estimatedTravel / segmentLength ) );

		// A terminating wall/actor collision limits the direct-flight search.  A
		// bounce does not: without simulating the contact normal, it cannot prove
		// where the projectile will go next and is kept conservative.
		const botCombatProjectileImpact_t impact = BotCombatFindProjectileImpact(
			projectile, self, lookAheadSeconds, steps );
		const float searchHorizon = impact.terminating ?
			idMath::ClampFloat( 0.0f, lookAheadSeconds, impact.time ) : lookAheadSeconds;

		idVec3 currentPoint;
		const float currentDistance = BotCombatProjectileDistanceAtTime( selfBounds,
			selfVelocity, projectileOrigin, projectileVelocity, projectileGravity,
			0.0f, &currentPoint );
		float closestTime = 0.0f;
		float closestDistance = currentDistance;
		idVec3 projectedProjectile = currentPoint;

		// Midpoint plus endpoint sampling keeps the maximum unsampled travel below
		// half a segment; gravity is evaluated exactly at every candidate.
		for ( int i = 1; i <= steps && searchHorizon > 0.0f; i++ ) {
			const float intervalStart = searchHorizon * ( (float)( i - 1 ) / (float)steps );
			const float intervalEnd = searchHorizon * ( (float)i / (float)steps );
			const float sampleTimes[] = {
				( intervalStart + intervalEnd ) * 0.5f,
				intervalEnd
			};

			for ( int sample = 0; sample < 2; sample++ ) {
				idVec3 point;
				const float distance = BotCombatProjectileDistanceAtTime( selfBounds,
					selfVelocity, projectileOrigin, projectileVelocity, projectileGravity,
					sampleTimes[sample], &point );
				if ( distance < closestDistance ) {
					closestDistance = distance;
					closestTime = sampleTimes[sample];
					projectedProjectile = point;
				}
			}
		}

		bool projectileThreat = false;
		float candidateTime = idMath::INFINITY;
		float candidateDistance = idMath::INFINITY;
		idVec3 candidatePoint = vec3_zero;

		const idVec3 relativeOrigin = projectileOrigin - selfBounds.GetCenter();
		const idVec3 relativeVelocity = projectileVelocity - selfVelocity;
		const bool initiallyClosing = ( relativeOrigin * relativeVelocity ) < 0.0f;
		const bool trajectoryApproaches = closestTime > 0.0f &&
			closestDistance + 0.5f < currentDistance;
		if ( closestDistance <= unsafeClearance &&
			 ( trajectoryApproaches || ( initiallyClosing && currentDistance <= unsafeClearance ) ) ) {
			projectileThreat = true;
			candidateTime = closestTime;
			candidateDistance = closestDistance;
			candidatePoint = projectedProjectile;
		}

		const float splashRadius = BotCombatProjectileSplashRadius( projectile );
		if ( impact.terminating ) {
			const bool impactThreat = impact.hitsSelf ||
				BotCombatSplashThreatensPlayer( self, impact.hit, impact.point,
					splashRadius, impact.time );
			if ( impactThreat && impact.time < candidateTime ) {
				projectileThreat = true;
				candidateTime = impact.time;
				candidateDistance = ( selfBounds + selfVelocity * impact.time ).ShortestDistance(
					impact.point );
				candidatePoint = impact.point;
			}
		}

		// A grenade that has settled beside the bot has no useful closest-motion
		// solution, but its queued fuse is exactly why the bot should leave.  The
		// event system does not expose remaining fuse time, so use a short warning
		// deadline only for nearly-resting, fuse-detonated splash ordnance.
		const bool armedFuse = splashRadius > 0.0f &&
			projectile->spawnArgs.GetBool( "detonate_on_fuse", "0" );
		if ( armedFuse && projectileVelocity.LengthSqr() <= Square( 8.0f ) ) {
			const float fuseWarningTime = Min( lookAheadSeconds, 0.25f );
			if ( BotCombatSplashThreatensPlayer( self, projectile, projectileOrigin,
					splashRadius, fuseWarningTime ) && fuseWarningTime < candidateTime ) {
				projectileThreat = true;
				candidateTime = fuseWarningTime;
				candidateDistance = ( selfBounds + selfVelocity * fuseWarningTime ).ShortestDistance(
					projectileOrigin );
				candidatePoint = projectileOrigin;
			}
		}

		if ( !projectileThreat ) {
			continue;
		}

		// Earliest danger is the most urgent.  Distance breaks ties between two
		// threats that mature on the same simulation tick.
		if ( candidateTime < bestTime ||
			 ( idMath::Fabs( candidateTime - bestTime ) <= idMath::FLOAT_EPSILON &&
			   candidateDistance < bestDistance ) ) {
			bestTime			= candidateTime;
			bestDistance		= candidateDistance;
			threatProjectile	= projectile;
			closestPoint		= candidatePoint;
		}
	}

	if ( !threatProjectile ) {
		return false;
	}

	timeToClosest = bestTime;
	return true;
}
