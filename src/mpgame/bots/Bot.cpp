//----------------------------------------------------------------
// Bot.cpp
//
// Multiplayer bot behaviour and user command synthesis.  See Bot.h.
//----------------------------------------------------------------

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"

// Game_local.h forward declares idProjectile but does not define it, and the
// projectile lead needs the two static accessors that read a launch speed and a
// gravity out of an entityDef dictionary.
#if !defined(__GAME_PROJECTILE_H__)
	#include "../Projectile.h"
#endif

#include "Bot.h"
#include "BotCombat.h"
#include "BotObjective.h"

rvBotManager				botManager;

/*
===============================================================================

	Tuning.

	Only the things that are the same for every bot live here.  Anything a
	server operator, a designer or a mod would ever want to argue about is a
	trait, and traits come from content - see BotCharacter.h.

===============================================================================
*/

static const int	BOT_REPATH_COMBAT_MSEC	= 750;		// chasing a player that keeps moving
static const int	BOT_REPATH_IDLE_MSEC	= 3000;		// heading for a fixed goal
static const float	BOT_CORNER_REACH		= 28.0f;	// horizontal distance that counts as "arrived"
static const float	BOT_CORNER_REACH_Z		= 24.0f;	// ordinary walk corners stay on their sampled floor
static const float	BOT_TRAVEL_ENTRY_REACH	= 12.0f;	// deliberate actions must be entered, not skipped a cell early
static const float	BOT_TRAVEL_REACH_Z		= 8.0f;		// landing confirmation is much tighter than ordinary steering
static const float	BOT_TRAVEL_OVERSHOOT	= 64.0f;	// a valid jump may land beyond its one-cell destination
static const int	BOT_TRAVEL_COMMIT_MSEC	= 1500;		// do not let generic unstick fight a traversal in progress
static const int	BOT_TRAVEL_TIMEOUT_MSEC	= 10000;	// a broken authored transport must still release its route
static const int	BOT_ENEMY_MEMORY_MSEC	= 3000;		// base for how long a lost enemy is chased, scaled by pursuit
static const float	BOT_ITEM_SEARCH_RANGE	= 2500.0f;	// base, scaled by itemFocus
static const int	BOT_STUCK_CHECK_MSEC	= 700;
static const int	BOT_UNSTICK_MSEC		= 900;
static const int	BOT_GOAL_GIVEUP_MSEC	= 12000;	// a goal this stale is not going to resolve
static const int	BOT_GOAL_AVOID_MSEC		= 20000;	// and how long an abandoned goal stays off the list
static const float	BOT_ITEM_ARRIVED		= 64.0f;	// close enough that a pickup would have fired
static const int	BOT_GOAL_SELECT_MSEC	= 250;		// how often the full goal scan may run
static const int	BOT_GOAL_COMMIT_MSEC	= 1800;		// suppress equivalent-goal chatter
static const float	BOT_GOAL_SWITCH_MARGIN	= 1.20f;	// challenger must be materially better
static const float	BOT_GOAL_PROGRESS_EPSILON = 24.0f;	// route shortening counts as progress
static const int	BOT_RECOVER_SEARCH_MSEC	= 500;		// how often the off-mesh search may run
static const float	BOT_RECOVER_RADIUS		= 1536.0f;	// how far off the mesh a bot will look for a way back
static const int	BOT_RECOVER_IDLE_MSEC	= 2000;		// no route this long means the bot is stranded
static const int	BOT_PATIENCE_HOLD_MSEC	= 4000;		// how long a maximally patient bot sits on a finished roam
static const float	BOT_DEFEND_RADIUS		= 320.0f;	// useful perimeter around an authored team base

// The aim point is biased up the target's own bounding box rather than by a
// fixed number of units, so it lands in the same place on any player model.  A
// competent shot puts it in the upper chest; a poor one drags it down into the
// legs and the floor.  weaponSkill carries this instead of a trait of its own:
// they are the same competence, and content that tunes one should not have to
// remember to tune the other.
static const float	BOT_AIM_HEIGHT_BIAS		= 0.20f;

// Second tremor octave.  Deliberately not a whole multiple, so the two never
// re-align into an audible-looking beat.
static const float	BOT_TREMOR_OCTAVE		= 2.37f;

// The measured angular rate of the target is low passed over roughly four
// frames.  Unfiltered it would chatter with every step the target takes and
// the tracking error would read as noise rather than as lag.
static const float	BOT_ANGVEL_FILTER		= 0.25f;

// The turn is integrated explicitly, and content can ask for a high turnAccel
// with a low turnSpeed, which makes the natural frequency arbitrarily large.
// Past roughly this much phase per frame an explicit integrator diverges, and
// the symptom is a bot whose view spins up until it is unwatchable.  Clamping
// the frequency degrades the response instead, which is merely wrong-looking.
static const float	BOT_TURN_STABLE_WN		= 0.5f;

// Seconds between shots below which a weapon counts as automatic and gets
// burst discipline.  Everything slower - rail, shotgun, rocket - already has
// its own rhythm and only obeys the settle time.
static const float	BOT_AUTO_FIRE_RATE		= 0.25f;

// Centre of the smooth close-to-far weapon blend.  Deliberately NOT
// botTraits_t::combatRange: that is the distance the bot WANTS to fight at.
// What weapon suits the actual range is a property of the weapon; personality
// biases choose within that continuous opinion.
static const float	BOT_WEAPON_RANGE_SPLIT	= 500.0f;

// A gap this short does not count as losing sight of someone: it is one
// dropped trace in a running fight, not a re-peek.
static const int	BOT_CONTINUOUS_MSEC		= 100;

// Being shot tells a bot where to look.  Without this the fov cone makes a bot
// standing with its back to a fight impossible to provoke, which is worse than
// the 360 degree vision it replaced.
static const int	BOT_ALERT_MSEC			= 1500;

static const int	BOT_DODGE_MSEC			= 700;		// how long one dodge lasts once it starts
static const int	BOT_THREAT_MEMORY_MSEC	= 2500;		// damage source remains a tactical threat
static const int	BOT_INVIS_REVEAL_MSEC	= 1200;		// a firing/damaging invisible player gives itself away
static const float	BOT_STRAFE_RANGE_SCALE	= 1.5f;		// dodge inside this multiple of the active fighting range
static const float	BOT_RETREAT_RANGE_SCALE	= 1.8f;		// how much further off a bot looking for health stands

static const float	BOT_ITEM_DENIED_RANGE	= 700.0f;	// close enough that the item was really taken from under it

static const int	BOT_KILL_STREAK			= 3;		// kills without dying that count as a streak
static const int	BOT_CHAT_JITTER_MSEC	= 600;		// natural variation after the length-based typing delay
static const int	BOT_CHAT_MIN_DELAY_MSEC	= 750;		// make even a one-word line visibly typed
static const int	BOT_CHAT_MAX_DELAY_MSEC	= 5000;		// keep long authored lines from taking a bot out for too long
static const float	BOT_CHAT_CHATTY_BONUS	= 0.35f;	// added to chatiness at bot_chat 2
static const int	BOT_LEADER_CHECK_MSEC	= 2000;		// how often the scoreboard is scanned for a lead change
static const int	BOT_REPLY_SOURCE_THROTTLE_MSEC = 8000;	// one speaker cannot work through the whole roster
static const float	BOT_REPLY_PLAYER_CHANCE	= 0.45f;
static const float	BOT_REPLY_PLAYER_ADDRESSED_CHANCE = 0.85f;
static const float	BOT_REPLY_BOT_CHANCE	= 0.12f;	// ordinary bot lines may start a conversation, rarely
static const float	BOT_REPLY_BOT_ADDRESSED_CHANCE = 0.30f;

// Weapon preference, best close-range score first.  Suitability blends across
// range so a bot does not try to snipe with a shotgun or fight a knife-range
// duel with a rail gun.  A style or character multiplies each entry through
// botTraits_t::weaponBias, so a sniper reaches for the rail gun in a corridor
// and a rusher does not.
typedef struct botWeaponRange_s {
	const char *name;
	float closeScore;
	float farScore;
	float preferredRange;
} botWeaponRange_t;

// Continuous close/far opinions avoid switching at one magic distance.  The
// current weapon receives hysteresis in UpdateWeapon; these scores describe
// only suitability.  Expansion weapons are first-class choices.
static const botWeaponRange_t botWeaponRanges[] = {
	{ "weapon_shotgun",          12.0f,  2.0f, 220.0f },
	{ "weapon_lightninggun",     11.0f,  8.0f, 420.0f },
	{ "weapon_rocketlauncher",   10.0f,  9.0f, 560.0f },
	{ "weapon_hyperblaster",      9.0f,  7.0f, 500.0f },
	{ "weapon_napalmgun",         8.0f,  4.0f, 400.0f },
	{ "weapon_nailgun",           7.0f,  6.0f, 520.0f },
	{ "weapon_dmg",               6.0f, 11.0f, 760.0f },
	{ "weapon_railgun",           5.0f, 12.0f, 900.0f },
	{ "weapon_machinegun",        4.0f,  5.0f, 600.0f },
	{ "weapon_grenadelauncher",   3.0f,  3.0f, 480.0f },
	{ "weapon_blaster",           2.0f,  2.0f, 480.0f },
	{ "weapon_gauntlet",          1.0f,  1.0f,  72.0f }
};
static const int BOT_NUM_WEAPON_RANGES = sizeof( botWeaponRanges ) / sizeof( botWeaponRanges[0] );

/*
================
BotPreferredCombatRange

Personality says what sort of fight a bot seeks; the weapon in its hands says
what sort of fight it can win right now.  Range discipline blends the two, so
a disciplined sniper with a shotgun closes the gap while a reckless rusher
with a rail gun still retains some of its authored character.
================
*/
static float BotPreferredCombatRange( const idPlayer *self, const botTraits_t &traits ) {
	if ( !self ) {
		return traits.combatRange;
	}

	const int currentSlot = self->GetCurrentWeapon();
	const char *currentWeapon = self->spawnArgs.GetString( va( "def_weapon%d", currentSlot ), "" );
	for ( int i = 0; i < BOT_NUM_WEAPON_RANGES; i++ ) {
		if ( idStr::Icmp( currentWeapon, botWeaponRanges[i].name ) != 0 ) {
			continue;
		}

		const float discipline = idMath::ClampFloat( 0.0f, 1.0f, traits.rangeDiscipline );
		return traits.combatRange +
			( botWeaponRanges[i].preferredRange - traits.combatRange ) * discipline;
	}

	return traits.combatRange;
}

/*
================
BotObjectiveRouteOrigin

An escort should protect the carrier, not occupy the carrier's collision hull.
Give each bot a stable trailing lane, then project that lane onto a proven nav
sample.  Tight corridors or unsupported offsets fall back to the authoritative
carrier origin; Repath performs the final directed reachability proof.
================
*/
static idVec3 BotObjectiveRouteOrigin( idPlayer *self, const botObjective_t &objective ) {
	idEntity *entity = objective.entity.GetEntity();
	if ( !self || objective.kind != BOTOBJ_ESCORT || !entity ||
		 !entity->IsType( idPlayer::GetClassType() ) || !navMesh.IsValid() ) {
		return objective.origin;
	}

	idPlayer *carrier = static_cast<idPlayer *>( entity );
	idVec3 forward = carrier->GetPhysics()->GetLinearVelocity();
	forward.z = 0.0f;
	if ( forward.Normalize() < 48.0f ) {
		idVec3 viewOrigin;
		idMat3 viewAxis;
		carrier->GetViewPos( viewOrigin, viewAxis );
		forward = viewAxis[0];
		forward.z = 0.0f;
		if ( forward.Normalize() <= 0.0f ) {
			return objective.origin;
		}
	}

	const int slot = self->entityNumber & 3;
	static const float sideOffsets[4] = { -96.0f, 96.0f, -160.0f, 160.0f };
	const float trailingDistance = ( slot < 2 ) ? 112.0f : 176.0f;
	const idVec3 side = forward.Cross( idVec3( 0.0f, 0.0f, 1.0f ) );
	const idVec3 candidate = objective.origin - forward * trailingDistance +
		side * sideOffsets[slot];

	const int node = navMesh.FindNearestNode( candidate, 192.0f, true );
	return ( node >= 0 ) ? navMesh.GetNode( node ).origin : objective.origin;
}

// Fallback roster, used when the character files are missing or bot_characters
// is off.  With characters loaded the names come from the roster instead, so
// the bot in the scoreboard and the personality behind it are the same thing.
static const char *botNames[] = {
	"Voss", "Bidwell", "Cortez", "Rhodes", "Sledge", "Morris", "Strauss", "Tetzlaff",
	"Marsh", "Hollenbeck", "Sorg", "Anderson", "Gunner", "Makron", "Kane", "Bagby",
	NULL
};

/*
================
BotDisplayName

Everything a chat line refers to by name goes through here.  ui_name is the
name every other player sees, which is what a taunt has to use.
================
*/
static const char *BotDisplayName( const idPlayer *player ) {
	if ( !player || player->entityNumber < 0 || player->entityNumber >= MAX_CLIENTS ) {
		return "";
	}

	return gameLocal.userInfo[player->entityNumber].GetString( "ui_name" );
}

/*
================
BotChatSendTime

Use visible characters rather than bytes so colour and formatting escapes cost
no typing time.  chatDelayScale slows the whole action for less capable or more
deliberate characters; the final cap keeps even the longest valid line a short
piece of flavour instead of a five-minute commitment.
================
*/
static int BotChatSendTime( const idStr &line, float delayScale ) {
	const int visibleCharacters = Max( 1, line.LengthWithoutEscapes() );
	const int charactersPerMinute = Max( 1, bot_chatCPM.GetInteger() );
	const float typingMsec = ( (float)visibleCharacters * 60000.0f ) / (float)charactersPerMinute;
	const float scaledMsec = ( bot_chatDelay.GetFloat() + typingMsec ) * delayScale;
	const int variedMsec = (int)scaledMsec + gameLocal.random.RandomInt( BOT_CHAT_JITTER_MSEC );
	const int delayMsec = idMath::ClampInt( BOT_CHAT_MIN_DELAY_MSEC, BOT_CHAT_MAX_DELAY_MSEC, variedMsec );

	return gameLocal.time + delayMsec;
}

/*
================
BotReadableName

Turns a class name into something that can be dropped into a chat line.
Localised inv_name values are refused on purpose: idMultiplayerGame::
ProcessChatMessage only resolves a string id when the WHOLE line starts with
'#', so a '#str_' substituted into the middle of one would be printed raw.

Every pickup prefix is tried rather than one the caller guesses: the thing a
bot was denied is as often a weapon_ or an ammo_ as an item_, and a wrong guess
leaves the prefix in the line - "weapon hyperblaster was mine" instead of
"Hyperblaster was mine".
================
*/
static idStr BotReadableName( const idDict &spawnArgs ) {
	static const char *pickupPrefixes[] = { "weapon_", "ammo_", "item_", "powerup_", NULL };

	idStr result = spawnArgs.GetString( "inv_name", "" );

	if ( result.IsEmpty() || result[0] == '#' ) {
		result = spawnArgs.GetString( "classname", "" );

		for ( int i = 0; pickupPrefixes[i]; i++ ) {
			if ( result.StripLeadingOnce( pickupPrefixes[i] ) ) {
				break;
			}
		}

		result.Replace( "_", " " );

		// Mid-sentence it still reads as a proper noun, which is how a player
		// would type it.
		if ( result.Length() ) {
			result[0] = idStr::ToUpper( result[0] );
		}
	}

	return result;
}

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
	initialTeamAssignmentPending = false;
	teamAssignment		= TEAM_NONE;
	teamAssignmentInstance = -1;

	character			= NULL;
	skillOverride		= -1;
	skillLevel			= -1;
	effectiveSkill		= 0.0f;

	// botTraits_t owns idStr weapon-bias entries.  Byte-zeroing it corrupts
	// those live C++ objects; ResolveTraits fills every scalar before use.
	traits.numWeaponBias	= 0;

	pathCorner			= 0;
	pathTime			= 0;
	goalType			= BOTGOAL_NONE;
	goalOrigin.Zero();
	goalGiveUpTime		= 0;
	goalUtility			= 0.0f;
	goalCommitUntil		= 0;
	goalBestDistance		= idMath::INFINITY;
	goalProgressTime		= 0;
	objectiveKind			= BOTOBJ_NONE;
	objectiveHoldPosition = false;
	repathFailures		= 0;
	enemyPathTime		= 0;
	nextGoalSelectTime	= 0;
	holdUntil			= 0;
	noRouteSince			= 0;
	traversalCorner		= -1;
	traversalStartOrigin.Zero();
	traversalStartTime	= 0;
	traversalEntered		= false;
	traversalStarted	= false;
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
	stuckChecks			= 0;
	stuckPathCorner		= -1;
	stuckCornerDistance	= idMath::INFINITY;

	enemyAcquiredTime	= 0;
	enemyLastSeenTime	= 0;
	enemyLastSeenOrigin.Zero();
	enemyLastSeenVelocity.Zero();
	enemyVisiblePoint.Zero();
	targetPickBest		= false;
	lastAttacker			= NULL;
	lastAttackerTime	= 0;

	aimAngles.Zero();
	aimRate.Zero();
	aimBelief.Zero();
	aimBeliefValid		= false;
	aimPoint.Zero();
	aimPointValid		= false;
	aimTrackAngles.Zero();
	aimTrackValid		= false;
	aimAngVel			= 0.0f;
	aimLeadRoll			= 1.0f;

	for ( int i = 0; i < 4; i++ ) {
		tremorPhase[i] = 0.0f;
	}

	enemyFireTime		= 0;
	onTargetTime		= 0;
	burstEndTime		= 0;
	burstRestTime		= 0;
	nextSingleShotTime	= 0;
	wasFiring			= false;

	mistake				= BOTMISTAKE_NONE;
	mistakeEndTime		= 0;
	nextMistakeTime		= 0;

	damageStamp			= 0;
	dodgeStartTime		= 0;
	dodgeEndTime			= 0;
	nextThreatScanTime	= 0;
	strafeSide			= 1.0f;
	strafeFlipTime		= 0;
	dodgeJump			= false;

	chatTeamOnly		= false;
	chatPendingIsReply = false;
	chatSendTime		= 0;
	killStreak			= 0;
	revengeTarget		= -1;
	announcedEntry		= false;

	nextWeaponTime		= 0;
	lastWeaponSwitchTime = 0;
	nextRejoinTime		= 0;
	nextStatusTime		= 0;
	nextAimDebugTime	= 0;
}

/*
================
rvBot::Init
================
*/
void rvBot::Init( int clientNum, const char *name, int skillOverride, bool requireExactCharacter ) {
	// A bot re-begun after a map change comes back through here on the same
	// slot, because bots hold their client slots across one.  It keeps the
	// personality it already had: characters describe opponents, and an
	// opponent who becomes somebody else between maps is not one.
	const bool rejoining = ( active && this->clientNum == clientNum );

	this->clientNum	= clientNum;
	this->name		= ( name && name[0] ) ? name : "bot";
	this->active	= true;

	// A newly allocated engine bot slot can inherit a perfectly valid-looking
	// ui_team from the server's template.  It is not an operator choice: addbot
	// has no team argument.  Mark exactly this first spawn for an authoritative
	// balance pass; a map rejoin keeps the side already assigned to the bot.
	if ( !rejoining ) {
		initialTeamAssignmentPending = true;
		teamAssignment = TEAM_NONE;
		teamAssignmentInstance = -1;
	}

	if ( skillOverride >= 1 && skillOverride <= BOT_SKILL_LEVELS ) {
		this->skillOverride = skillOverride;
	} else if ( !rejoining ) {
		this->skillOverride = -1;
	}

	const int level = idMath::ClampInt( 1, BOT_SKILL_LEVELS,
										( this->skillOverride > 0 ) ? this->skillOverride : bot_skill.GetInteger() );

	// Claim a personality.  PickCharacter honours bot_forceCharacter and hands
	// out unused characters first, so a full server is a cast rather than eight
	// copies of the same opponent.  A NULL answer - no roster, or
	// bot_characters 0 - is normal and leaves the bot on the plain skill curve.
	if ( !rejoining ) {
		if ( character ) {
			botCharacterManager.ReleaseCharacter( character );
			character = NULL;
		}
		if ( bot_characters.GetBool() ) {
			const rvBotCharacter *requestedCharacter = botCharacterManager.FindCharacter( this->name.c_str() );

			if ( requestedCharacter && !requestedCharacter->IsInUse() ) {
				character = requestedCharacter;
				botCharacterManager.MarkCharacterUsed( character );
			} else if ( !requireExactCharacter ) {
				character = botCharacterManager.PickCharacter( level );
			}
		}
	}

	// The display name follows the character, not the other way round: the slot
	// was allocated from a peek at the same roster, and FillUserInfo republishes
	// ui_name on every userinfo update, so this is what the scoreboard shows.
	if ( character ) {
		this->name = character->GetName();
	}

	// The tremor phases are seeded from the client number rather than rolled,
	// so a bot shakes the same way for its whole life and no two bots on a
	// server shake in step.
	for ( int i = 0; i < 4; i++ ) {
		tremorPhase[i] = (float)( clientNum + 1 ) * 1.7f + (float)i * 2.3f;
	}

	killStreak		= 0;
	revengeTarget	= -1;

	ResolveTraits();

	// One hello per bot, not one per respawn.  The idPlayer does not exist yet
	// when OnSpawnPlayer runs, but the line is queued and does not go out for
	// at least bot_chatDelay, by which time it does.
	if ( !announcedEntry ) {
		announcedEntry = true;
		QueueChat( BOTCHAT_ENTERGAME, NULL, NULL, NULL );
	}

	OnSpawn();
}

/*
================
rvBot::RebindCharacter

rvBotCharacterManager::Reload deletes every parsed character, so the pointer
this bot is holding is freed memory the moment it returns.  It does not reliably
crash: the new roster is parsed straight back into the same heap, so a stale
pointer usually lands on a DIFFERENT character at the same address and the bot
silently starts wearing someone else's personality.  Re-look-up by name is the
only thing that survives a reparse, and the name is authoritative because
rvBot::Init copies it out of the character in the first place.

A name that no longer exists in the roster leaves the bot on the plain skill
curve rather than inventing a replacement - the same state bot_characters 0
produces, which is known to work.
================
*/
void rvBot::RebindCharacter( void ) {
	if ( !active ) {
		return;
	}

	// Not released first: the pointer is already dangling, and the fresh roster
	// starts with nothing marked in use, so there is nothing to give back.
	character = NULL;

	if ( bot_characters.GetBool() ) {
		character = botCharacterManager.FindCharacter( name.c_str() );

		if ( character ) {
			botCharacterManager.MarkCharacterUsed( character );
		}
	}

	ResolveTraits();
}

/*
================
rvBot::Shutdown
================
*/
void rvBot::Shutdown( void ) {
	idPlayer *self = GetPlayer();

	if ( self ) {
		self->isChatting = false;
	}

	// The identity goes back in the pool for the next bot, or the roster drains
	// after a few kicks and every later bot falls through to the "any character
	// at all" case.
	if ( character ) {
		botCharacterManager.ReleaseCharacter( character );
		character = NULL;
	}

	active			= false;
	clientNum		= -1;
	initialTeamAssignmentPending = false;
	teamAssignment	= TEAM_NONE;
	teamAssignmentInstance = -1;
	announcedEntry	= false;
	killStreak		= 0;
	revengeTarget	= -1;
	chatPending.Clear();
	chatTeamOnly	= false;
	chatPendingIsReply = false;
	chatSendTime	= 0;
	path.Clear();
	ResetTraversal();
	goalEntity		= NULL;
	enemy			= NULL;
}

/*
================
rvBot::ResolveTraits

The whole personality in one call: the skill curve for the level this bot plays
at, then its style, then its character, then their per-level overrides.

Called once per spawn and never per frame.  That is deliberate - a mid-match
edit of bot_skill, bot_skillVariance or bot_characters lands on the next
respawn, so nobody's aim changes in the middle of the fight they are already in.
================
*/
void rvBot::ResolveTraits( void ) {
	// An explicit per-bot skill from "addbot <name> <skill>" bypasses the
	// variance as well: an operator who asked for a level 2 bot asked for a
	// level 2 bot, not for a roll around one.
	const int	level		= idMath::ClampInt( 1, BOT_SKILL_LEVELS,
												( skillOverride > 0 ) ? skillOverride : bot_skill.GetInteger() );
	const float	variance	= ( skillOverride > 0 ) ? 0.0f : bot_skillVariance.GetFloat();

	skillLevel		= level;
	effectiveSkill	= rvBotCharacterManager::EffectiveSkill( level, variance, clientNum );

	botCharacterManager.ResolveTraits( bot_characters.GetBool() ? character : NULL,
									   level, variance, clientNum, traits );
}

/*
================
rvBot::OnSpawn
================
*/
void rvBot::OnSpawn( void ) {
	ResolveTraits();

	path.Clear();
	pathCorner			= 0;
	pathTime			= 0;
	goalType			= BOTGOAL_NONE;
	goalEntity			= NULL;
	goalGiveUpTime		= 0;
	goalUtility			= 0.0f;
	goalCommitUntil		= 0;
	goalBestDistance		= idMath::INFINITY;
	goalProgressTime		= gameLocal.time;
	objectiveKind			= BOTOBJ_NONE;
	objectiveHoldPosition = false;
	repathFailures		= 0;
	enemyPathTime		= 0;
	nextGoalSelectTime	= 0;
	holdUntil			= 0;
	noRouteSince			= gameLocal.time;
	ResetTraversal();
	recoverValid		= false;
	recoverSearchTime	= 0;
	avoidNext			= 0;

	for ( int i = 0; i < MAX_AVOID_GOALS; i++ ) {
		avoidGoals[i].until = 0;
	}

	enemy				= NULL;
	enemyAcquiredTime	= 0;
	enemyLastSeenTime	= 0;
	enemyLastSeenVelocity.Zero();
	enemyVisiblePoint.Zero();
	lastAttacker			= NULL;
	lastAttackerTime	= 0;
	enemyFireTime		= 0;
	onTargetTime		= 0;
	burstEndTime		= 0;
	burstRestTime		= 0;
	nextSingleShotTime	= 0;
	wasFiring			= false;
	stuckTime			= gameLocal.time;
	unstickUntil		= 0;
	stuckChecks			= 0;
	stuckPathCorner		= -1;
	stuckCornerDistance	= idMath::INFINITY;
	nextWeaponTime		= 0;
	lastWeaponSwitchTime = 0;

	aimRate.Zero();
	aimBeliefValid		= false;
	aimPointValid		= false;
	aimTrackValid		= false;
	aimAngVel			= 0.0f;
	aimLeadRoll			= 1.0f;

	mistake				= BOTMISTAKE_NONE;
	mistakeEndTime		= 0;
	nextMistakeTime		= 0;

	damageStamp			= 0;
	dodgeStartTime		= 0;
	dodgeEndTime			= 0;
	nextThreatScanTime	= 0;
	strafeFlipTime		= 0;
	dodgeJump			= false;

	// Whether this bot picks the best enemy or merely the nearest one is a
	// property of the engagement, not of the frame, so it is rolled here and
	// again on every fresh acquisition.
	targetPickBest		= ( gameLocal.random.RandomFloat() < traits.targetSelection );

	// A pending chat line deliberately survives: a bot that dies one second
	// after a frag should still get its line out, and the death event queues
	// over the top of it anyway.

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
void rvBot::FillUserInfo( idDict &info ) {
	info.Set( "ui_name", name.c_str() );
	info.SetBool( "ui_autoJoin", true );
	info.SetBool( "ui_joined", true );
	info.Set( "ui_spectate", "Play" );
	info.Set( "ui_ready", "Ready" );
	info.Set( "ui_showGun", "1" );

	if ( !info.GetInt( "ui_handicap" ) ) {
		info.SetInt( "ui_handicap", 100 );
	}

	// A character can carry a look as well as a voice.  All three keys are
	// written whatever the game type, because idPlayer::UpdateModelSetup reads
	// the one that matches the side it ends up on and ignores the rest - which
	// also means this does not have to run after the team block below.
	//
	// The name is validated first.  UpdateModelSetup silently replaces an
	// unknown model AND writes the replacement back into the userinfo, which on
	// a server buffers an updateUI command; since every bot userinfo update
	// comes back through here, a bad name would loop forever.
	if ( character && bot_characters.GetBool() ) {
		static const char *modelKeys[3]	= { "ui_model", "ui_model_marine", "ui_model_strogg" };
		const char *modelNames[3]		= { character->GetModel(), character->GetModelMarine(), character->GetModelStrogg() };

		for ( int i = 0; i < 3; i++ ) {
			if ( !modelNames[i][0] ) {
				continue;
			}
			if ( !declManager->FindType( DECL_PLAYER_MODEL, modelNames[i], false ) ) {
				gameLocal.Warning( "bot character '%s': no player model '%s'", character->GetName(), modelNames[i] );
				continue;
			}

			info.Set( modelKeys[i], modelNames[i] );
		}
	}

	// The first assignment must ignore ui_team completely.  Bot slots inherit
	// server userinfo defaults, so a non-empty value is not evidence that anyone
	// selected that side.  Later valid values are authoritative (ShuffleTeams
	// and SwitchToTeam use them), while an engine update that omits ui_team is
	// repaired from the reservation instead of triggering another rebalance.
	if ( gameLocal.IsTeamGame() ) {
		int requestedTeam = TEAM_NONE;
		const char *requestedName = info.GetString( "ui_team" );

		for ( int team = 0; team < TEAM_MAX; team++ ) {
			if ( !idStr::Icmp( requestedName, idMultiplayerGame::teamNames[team] ) ) {
				requestedTeam = team;
				break;
			}
		}

		if ( initialTeamAssignmentPending || teamAssignment < 0 || teamAssignment >= TEAM_MAX ) {
			teamAssignment = botManager.BalancedTeamForBot( clientNum, teamAssignmentInstance );
			initialTeamAssignmentPending = false;
		} else if ( requestedTeam >= 0 && requestedTeam < TEAM_MAX ) {
			teamAssignment = requestedTeam;

			idPlayer *self = GetPlayer();
			if ( self ) {
				teamAssignmentInstance = self->GetInstance();
			}
		}

		info.Set( "ui_team", idMultiplayerGame::teamNames[teamAssignment] );
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
	if ( other->GetInstance() != self->GetInstance() ||
		 !gameLocal.mpGame.CanPlay( other ) || other->spectating || other->health <= 0 ) {
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
bool rvBot::CanSee( idPlayer *self, idEntity *other, idVec3 *visiblePoint ) const {
	if ( !other || !other->IsType( idPlayer::GetClassType() ) ) {
		return false;
	}

	idVec3 point;
	if ( !BotCombatFindVisibleAimPoint( self, static_cast<idPlayer *>( other ), point ) ) {
		return false;
	}
	if ( visiblePoint ) {
		*visiblePoint = point;
	}
	return true;
}

/*
================
rvBot::WantsHealth
================
*/
bool rvBot::WantsHealth( idPlayer *self ) const {
	const float maxHealth = (float)Max( 1, self->inventory.maxHealth );

	return ( (float)self->health < traits.retreatHealth * maxHealth );
}

/*
================
rvBot::AtObjectiveHoldPosition

Control zones use their authored trigger bounds and Freeze Tag uses the same
thaw radius as the rules.  Other hold objectives (principally waiting at a CTF
base) use a small arrival volume around the resolved objective origin.
================
*/
bool rvBot::AtObjectiveHoldPosition( idPlayer *self ) const {
	if ( !self || goalType != BOTGOAL_OBJECTIVE || !objectiveHoldPosition ) {
		return false;
	}

	idEntity *objectiveEntity = goalEntity.GetEntity();
	if ( !objectiveEntity || objectiveEntity->GetInstance() != self->GetInstance() ||
		 !objectiveEntity->GetPhysics() ) {
		return false;
	}

	if ( objectiveKind == BOTOBJ_CONTROL ) {
		return objectiveEntity->GetPhysics()->GetAbsBounds().IntersectsBounds(
			self->GetPhysics()->GetAbsBounds() );
	}

	idVec3 delta = objectiveEntity->GetPhysics()->GetOrigin() - self->GetPhysics()->GetOrigin();
	if ( objectiveKind == BOTOBJ_DEFEND ) {
		const float height = idMath::Fabs( delta.z );
		delta.z = 0.0f;
		return height <= 128.0f && delta.LengthSqr() <= BOT_DEFEND_RADIUS * BOT_DEFEND_RADIUS;
	}
	if ( objectiveKind == BOTOBJ_RESCUE ) {
		const float thawRadius = (float)Max( 1, gameLocal.serverInfo.GetInt( "si_freezeThawRadius" ) );
		return delta.LengthSqr() <= thawRadius * thawRadius;
	}

	const float height = idMath::Fabs( delta.z );
	delta.z = 0.0f;
	return height <= BOT_CORNER_REACH_Z * 2.0f &&
		delta.LengthSqr() <= BOT_ITEM_ARRIVED * BOT_ITEM_ARRIVED;
}

/*
================
BotCarriesScoringObjective

Keep combat's small piece of objective awareness independent of objective
route discovery.  These are the carried states in which an enemy nearby is an
immediate threat to a teammate completing the rule.
================
*/
static bool BotCarriesScoringObjective( const idPlayer *player ) {
	return player &&
		( player->PowerUpActive( POWERUP_CTF_MARINEFLAG ) ||
		  player->PowerUpActive( POWERUP_CTF_STROGGFLAG ) ||
		  player->PowerUpActive( POWERUP_CTF_ONEFLAG ) ||
		  player->PowerUpActive( POWERUP_DEADZONE ) );
}

/*
================
rvBot::TargetScore

Lower is better.  Distance dominates - a bot that walks past someone shooting
it to reach a weaker player at the far end of the map is not playing well - but
a hurt target is worth turning towards.
================
*/
float rvBot::TargetScore( idPlayer *other, float distSqr ) const {
	const float effectiveHealth = (float)Max( 0, other->health ) + (float)Max( 0, other->inventory.armor ) * 0.66f;
	const float effectiveMaximum = (float)Max( 1, other->inventory.maxHealth ) +
							   (float)Max( 0, other->inventory.maxarmor ) * 0.66f;
	const float health = idMath::ClampFloat( 0.0f, 1.0f, effectiveHealth / effectiveMaximum );
	const float healthBias = 1.0f + traits.opportunism * ( health - 0.5f );
	const float revengeBias = ( other->entityNumber == revengeTarget ) ?
							  1.0f - 0.35f * traits.vengefulness : 1.0f;
	const float attackerBias = ( other == lastAttacker.GetEntity() &&
								 gameLocal.time - lastAttackerTime < BOT_THREAT_MEMORY_MSEC ) ? 0.55f : 1.0f;
	const bool carrier = BotCarriesScoringObjective( other );
	const float objectiveBias = carrier ? 0.45f : 1.0f;

	// Protect a friendly carrier by preferring enemies already close enough to
	// intercept it.  The bias is smooth, so a merely visible enemy does not
	// steal attention from the opponent directly on the carrier's heels.
	float carrierThreatBias = 1.0f;
	idPlayer *self = GetPlayer();
	if ( self && gameLocal.IsTeamGame() ) {
		static const float protectRange = 900.0f;
		for ( int i = 0; i < gameLocal.numClients; i++ ) {
			idPlayer *teamMate = gameLocal.GetClientByNum( i );
			if ( !teamMate || teamMate->spectating || teamMate->health <= 0 ||
				 teamMate->GetInstance() != self->GetInstance() || teamMate->team != self->team ||
				 !BotCarriesScoringObjective( teamMate ) ) {
				continue;
			}

			const float threatDistance =
				( other->GetPhysics()->GetOrigin() - teamMate->GetPhysics()->GetOrigin() ).Length();
			if ( threatDistance < protectRange ) {
				carrierThreatBias = Min( carrierThreatBias,
					0.55f + 0.45f * threatDistance / protectRange );
			}
		}
	}
	const bool powered = other->PowerUpActive( POWERUP_QUADDAMAGE ) ||
						 other->PowerUpActive( POWERUP_HASTE ) ||
						 other->PowerUpActive( POWERUP_REGENERATION ) ||
						 other->PowerUpActive( POWERUP_GUARD ) ||
						 other->PowerUpActive( POWERUP_DOUBLER ) ||
						 other->PowerUpActive( POWERUP_SCOUT );
	const float powerupBias = powered ? 0.68f : 1.0f;
	const bool firing = gameLocal.usercmds && other->entityNumber >= 0 &&
					other->entityNumber < gameLocal.numClients &&
					( gameLocal.usercmds[other->entityNumber].buttons & BUTTON_ATTACK );
	const float firingBias = firing ? 0.82f : 1.0f;

	return distSqr * healthBias * revengeBias * attackerBias * objectiveBias *
		carrierThreatBias * powerupBias * firingBias;
}

/*
================
rvBot::RollMistake

Rolled on acquisition and again every mistakeMsec while a fight lasts, so a
long engagement is a sequence of chances to get something wrong rather than one.
================
*/
void rvBot::RollMistake( void ) {
	nextMistakeTime = gameLocal.time + (int)traits.mistakeMsec;

	if ( gameLocal.random.RandomFloat() >= traits.mistakeChance ) {
		mistake			= BOTMISTAKE_NONE;
		mistakeEndTime	= 0;
		return;
	}

	mistake			= BOTMISTAKE_LOSETRACK + gameLocal.random.RandomInt( BOTMISTAKE_NUM - BOTMISTAKE_LOSETRACK );
	mistakeEndTime	= gameLocal.time + (int)traits.mistakeMsec;
}

/*
================
rvBot::MistakeActive
================
*/
bool rvBot::MistakeActive( int mistake ) const {
	return ( this->mistake == mistake && gameLocal.time < mistakeEndTime );
}

/*
================
rvBot::AcquireEnemy

Everything that happens once, at the moment a target registers.  The reaction
deadline is computed here rather than tested per frame, which is what makes a
re-acquisition cost something: the old code measured the delay from a timestamp
that only moved when the enemy had been gone for half a second, so inside a
continuous fight the reaction was zero forever.
================
*/
void rvBot::AcquireEnemy( idPlayer *self, idPlayer *foe, bool fresh ) {
	idVec3	eye;
	idMat3	axis;

	self->GetViewPos( eye, axis );

	enemy				= foe;
	enemyAcquiredTime	= gameLocal.time;

	idVec3 dir = foe->GetPhysics()->GetAbsBounds().GetCenter() - eye;

	float offAxis = 0.0f;
	if ( dir.Normalize() > 0.0f ) {
		offAxis = RAD2DEG( idMath::ACos( idMath::ClampFloat( -1.0f, 1.0f, dir * axis[0] ) ) );
	}

	// Something at the edge of vision registers late; something dead ahead does
	// not.  Linear in the angle, so it reads as a gradient rather than as a
	// switch that flips at the edge of the cone.
	const float periph	= traits.peripheralPenaltyMsec *
						  idMath::ClampFloat( 0.0f, 1.0f, offAxis / Max( traits.peripheralAngle, 1.0f ) );
	const float base	= fresh ? traits.reactionMsec : traits.reactionMsec * traits.reacquireFraction;
	const float jitter	= gameLocal.random.CRandomFloat() * traits.reactionVarianceMsec;
	const float initiativeScale = 1.5f - traits.initiative;

	enemyFireTime	= gameLocal.time + Max( 0, (int)( ( base + jitter + periph ) * initiativeScale ) );
	onTargetTime	= 0;
	burstEndTime	= 0;
	burstRestTime	= 0;

	if ( fresh ) {
		// A new engagement starts with the aim believing the truth rather than
		// wherever the last fight ended, and with its own roll of the dice.
		aimBeliefValid	= false;
		aimTrackValid	= false;
		aimAngVel		= 0.0f;
		aimLeadRoll		= 1.0f + gameLocal.random.CRandomFloat() * traits.aimLeadError;
		RollMistake();
	}

	if ( bot_debugAim.GetInteger() >= 1 ) {
		gameLocal.Printf( "bot %s: %s %s at %.0f deg off centre, may fire in %d ms\n",
						  name.c_str(), fresh ? "sighted" : "re-sighted",
						  BotDisplayName( foe ), offAxis, enemyFireTime - gameLocal.time );
	}
}

/*
================
rvBot::UpdateEnemy
================
*/
void rvBot::UpdateEnemy( idPlayer *self ) {
	idVec3	eye;
	idMat3	axis;

	self->GetViewPos( eye, axis );

	const idVec3	origin		= self->GetPhysics()->GetOrigin();
	const float		sightSqr	= traits.sightRange * traits.sightRange;

	// A bot used to notice players through the back of its own head.  fov is a
	// full cone, so half of it is what the view direction is tested against.
	const float		cosFov		= idMath::Cos( DEG2RAD( idMath::ClampFloat( 1.0f, 180.0f, traits.fov * 0.5f ) ) );
	idPlayer *	nearest			= NULL;
	float		nearestDistSqr	= sightSqr;
	idPlayer *	best			= NULL;
	float		bestScore		= idMath::INFINITY;
	idPlayer *	current			= enemy.GetEntity();
	float		currentScore	= idMath::INFINITY;

	for ( int i = 0; i < gameLocal.numClients; i++ ) {
		idEntity *ent = gameLocal.entities[i];

		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}

		idPlayer *other = static_cast<idPlayer *>( ent );
		if ( !IsEnemy( self, other ) ) {
			continue;
		}

		const float distSqr = ( other->GetPhysics()->GetOrigin() - origin ).LengthSqr();
		if ( distSqr >= sightSqr ) {
			continue;
		}

		const bool damagedUs = ( other == lastAttacker.GetEntity() &&
								 gameLocal.time - lastAttackerTime < BOT_ALERT_MSEC );
		const bool invisRevealed = ( other == lastAttacker.GetEntity() &&
									gameLocal.time - lastAttackerTime < BOT_INVIS_REVEAL_MSEC );
		const bool firing = gameLocal.usercmds && other->entityNumber >= 0 &&
						other->entityNumber < gameLocal.numClients &&
						( gameLocal.usercmds[other->entityNumber].buttons & BUTTON_ATTACK );
		if ( other->PowerUpActive( POWERUP_INVISIBILITY ) &&
			 !invisRevealed && !( firing && distSqr < 700.0f * 700.0f ) ) {
			continue;
		}

		idVec3 dir = other->GetPhysics()->GetAbsBounds().GetCenter() - eye;
		if ( dir.Normalize() <= 0.0f ) {
			continue;
		}
		if ( !damagedUs && dir * axis[0] < cosFov ) {
			continue;
		}
		idVec3 visiblePoint;
		if ( !CanSee( self, other, &visiblePoint ) ) {
			continue;
		}

		const float score = TargetScore( other, distSqr );

		if ( distSqr < nearestDistSqr ) {
			nearestDistSqr	= distSqr;
			nearest			= other;
		}
		if ( score < bestScore ) {
			bestScore	= score;
			best		= other;
		}
		if ( other == current ) {
			currentScore = score;
		}

		// Cache the sampled exposed point for the eventual winner below.  This is
		// overwritten only for that winner before aim consumes it.
		if ( other == current ) {
			enemyVisiblePoint = visiblePoint;
		}
	}

	idPlayer *pick = targetPickBest ? best : nearest;

	// Stay on the target already being shot at unless the alternative is enough
	// better for this personality.  targetStickiness 0 changes readily, 1 only
	// yields to an opponent scoring less than half as much.  The neutral 0.5
	// resolves to the original 0.75 margin.
	if ( current && currentScore < idMath::INFINITY && pick != current ) {
		const float pickScore = pick ? TargetScore( pick, ( pick->GetPhysics()->GetOrigin() - origin ).LengthSqr() ) : idMath::INFINITY;
		const float switchMargin = 1.0f - 0.5f * traits.targetStickiness;

		if ( pickScore > currentScore * switchMargin ) {
			pick = current;
		}
	}

	if ( pick ) {
		const int	gap		= gameLocal.time - enemyLastSeenTime;
		const bool	same	= ( current == pick );

		// Three cases, and the middle one is the whole point: a different
		// player or a long absence is a fresh sighting and costs the full
		// reaction; a short absence still costs reacquireFraction of it; only
		// an unbroken line of sight costs nothing.
		if ( !same || gap > BOT_CONTINUOUS_MSEC ) {
			AcquireEnemy( self, pick, !same || gap > (int)traits.reacquireMsec );
		} else if ( gameLocal.time >= nextMistakeTime ) {
			RollMistake();
		}

		enemyLastSeenTime	= gameLocal.time;
		enemyLastSeenOrigin	= pick->GetPhysics()->GetOrigin();
		enemyLastSeenVelocity = pick->GetPhysics()->GetLinearVelocity();
		CanSee( self, pick, &enemyVisiblePoint );
		return;
	}

	// Keep chasing a lost enemy for a moment, then forget it.  How long that
	// moment lasts is the personality's: a hunter follows someone through two
	// rooms, a roamer has already moved on to the next weapon.
	const int memory = (int)( BOT_ENEMY_MEMORY_MSEC * ( 0.5f + traits.pursuit ) );

	if ( current && ( !IsEnemy( self, current ) || gameLocal.time - enemyLastSeenTime > memory ) ) {
		enemy			= NULL;
		aimBeliefValid	= false;
		aimTrackValid	= false;
		onTargetTime	= 0;
		targetPickBest = ( gameLocal.random.RandomFloat() < traits.targetSelection );
	}
}

/*
================
rvBot::EnemyPursuitOrigin

A chase route should meet a moving opponent, not repeatedly terminate where
the opponent was at the last scan.  Visible opponents receive a short forward
intercept.  Once sight is lost, prediction grows only briefly from the last
observed velocity and fades with memory age; it never reads live movement
through a wall.  Vertical launch velocity is deliberately ignored because the
navigation goal is the floor the opponent is travelling across.
================
*/
idVec3 rvBot::EnemyPursuitOrigin( void ) const {
	idVec3 predicted = enemyLastSeenOrigin;
	const float age = Max( 0.0f, MS2SEC( gameLocal.time - enemyLastSeenTime ) );
	const bool visible = ( gameLocal.time == enemyLastSeenTime );

	float predictionSeconds;
	if ( visible ) {
		predictionSeconds = 0.20f + 0.30f * traits.pursuit;
	} else {
		const float confidence = idMath::ClampFloat( 0.0f, 1.0f, 1.0f - age / 3.0f );
		predictionSeconds = Min( age, 0.75f ) * traits.pursuit * confidence;
	}

	idVec3 lead = enemyLastSeenVelocity * predictionSeconds;
	lead.z = 0.0f;
	const float leadDistance = lead.Length();
	if ( leadDistance > 256.0f ) {
		lead *= 256.0f / leadDistance;
	}

	return predicted + lead;
}

/*
================
rvBot::ResetTraversal

The route owns the traversal state.  Any route replacement or abandonment must
drop it, or corner zero in the new route can inherit a completed jump from the
old one.
================
*/
void rvBot::ResetTraversal( void ) {
	traversalCorner		= -1;
	traversalStartOrigin.Zero();
	traversalStartTime	= 0;
	traversalEntered		= false;
	traversalStarted	= false;
}

/*
================
rvBot::Repath
================
*/
bool rvBot::Repath( idPlayer *self, const idVec3 &goal, bool preserveProgress ) {
	pathTime	= gameLocal.time;
	pathCorner	= 0;
	stuckPathCorner = 0;
	stuckCornerDistance = idMath::INFINITY;
	holdUntil	= 0;		// somewhere to be again, so no longer standing and watching
	ResetTraversal();

	if ( !navMesh.FindPath( self->GetPhysics()->GetOrigin(), goal, path ) ) {
		path.Clear();
		repathFailures++;
		if ( !noRouteSince ) {
			noRouteSince = gameLocal.time;
		}

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
	noRouteSince = 0;
	// Refreshing a moving target's route is not evidence that the bot moved.
	// Keep the original progress clock so a reachable-but-stalled chase can
	// still mature into the normal give-up/avoid path.
	if ( !preserveProgress ) {
		goalBestDistance = PathDistanceRemaining( self->GetPhysics()->GetOrigin() );
		goalProgressTime = gameLocal.time;
	}

	return true;
}

/*
================
rvBot::PathDistanceRemaining

Measures progress along the route rather than displacement in the room.  A bot
circling a pillar can travel a long way while getting no closer to its corner;
that is not progress and must eventually trigger recovery.
================
*/
float rvBot::PathDistanceRemaining( const idVec3 &origin ) const {
	if ( pathCorner < 0 || pathCorner >= path.Num() ) {
		return 0.0f;
	}

	float distance = ( path[pathCorner].origin - origin ).Length();
	for ( int i = pathCorner; i + 1 < path.Num(); i++ ) {
		distance += ( path[i + 1].origin - path[i].origin ).Length();
	}

	return distance;
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
	goalUtility		= 0.0f;
	goalCommitUntil	= 0;
	goalBestDistance	= idMath::INFINITY;
	goalProgressTime	= 0;
	objectiveKind		= BOTOBJ_NONE;
	objectiveHoldPosition = false;
	pathCorner		= 0;
	pathTime		= 0;
	ResetTraversal();
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

		const int node = navMesh.FindNearestNode( origin, BOT_RECOVER_RADIUS, true );

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
rvBot::ItemUtility

Inventory-aware value for a live pickup.  A full-health bot ignores routine
health, a stocked player stops queueing on unusable ammo, and a missing weapon
or major powerup remains worth crossing a room for.  Flags and the DeadZone
artifact are reserved for the objective planner.
================
*/
float rvBot::ItemUtility( idPlayer *self, idItem *item ) const {
	if ( !self || !item || item->IsType( rvItemCTFFlag::GetClassType() ) ||
		 item->IsType( riDeadZonePowerup::GetClassType() ) ) {
		return 0.0f;
	}

	float utility = 0.0f;
	const int healthAmount = item->spawnArgs.GetInt( "inv_health", "0" );
	if ( healthAmount > 0 ) {
		const int maxHealth = Max( 1, self->inventory.maxHealth );
		const int deficit = Max( 0, maxHealth - self->health );
		if ( deficit > 0 ) {
			utility = 1.0f + 4.0f * idMath::ClampFloat( 0.0f, 1.0f,
				(float)Min( deficit, healthAmount ) / (float)maxHealth );
			if ( WantsHealth( self ) ) {
				utility *= 2.2f;
			}
		} else if ( healthAmount >= 50 && self->health < maxHealth * 2 ) {
			// Mega-style health remains useful as an overstack; small packs do not.
			utility = 1.25f;
		}
	}

	const int armorAmount = item->spawnArgs.GetInt( "inv_armor", "0" );
	if ( armorAmount > 0 ) {
		const int maxArmor = Max( 1, self->inventory.maxarmor );
		const int deficit = Max( 0, maxArmor - self->inventory.armor );
		if ( deficit > 0 ) {
			utility = Max( utility, 0.8f + 3.0f * idMath::ClampFloat( 0.0f, 1.0f,
				(float)Min( deficit, armorAmount ) / (float)maxArmor ) );
		}
	}

	const char *weaponName = item->spawnArgs.GetString( "inv_weapon", "" );
	if ( weaponName[0] ) {
		const int slot = self->GetWeaponIndex( weaponName );
		const bool validSlot = slot >= 0 && slot < MAX_WEAPONS &&
			idStr::Icmp( self->spawnArgs.GetString( va( "def_weapon%d", slot ), "" ), weaponName ) == 0;
		if ( validSlot ) {
			const bool owned = ( self->inventory.weapons & ( 1 << slot ) ) != 0;
			if ( !owned ) {
				utility = Max( utility, 5.0f );
			}
		}
	}

	for ( const idKeyValue *ammo = item->spawnArgs.MatchPrefix( "inv_ammo_", NULL );
		  ammo != NULL; ammo = item->spawnArgs.MatchPrefix( "inv_ammo_", ammo ) ) {
		idStr ammoClass = ammo->GetKey();
		ammoClass.StripLeadingOnce( "inv_" );
		const int ammoIndex = self->inventory.AmmoIndexForAmmoClass( ammoClass.c_str() );
		const int maxAmmo = self->inventory.MaxAmmoForAmmoClass( self, ammoClass.c_str() );
		if ( ammoIndex > 0 && ammoIndex < MAX_AMMO && maxAmmo > 0 &&
			 self->inventory.ammo[ammoIndex] < maxAmmo ) {
			const float need = 1.0f - idMath::ClampFloat( 0.0f, 1.0f,
				(float)self->inventory.ammo[ammoIndex] / (float)maxAmmo );
			utility = Max( utility, 0.6f + 2.4f * need );
		}
	}

	if ( item->IsType( idItemPowerup::GetClassType() ) ) {
		utility = Max( utility, 6.0f );
	}

	return utility;
}

typedef struct botItemCandidate_s {
	idItem *	item;
	float	utility;
	float	cheapScore;
} botItemCandidate_t;

/*
================
rvBot::PickItemGoal

Routes only the strongest straight-line shortlist, then chooses by complete
route length and team claims.  This retains directed-path correctness without
running A* for every item on every decision tick.
================
*/
idEntity *rvBot::PickItemGoal( idPlayer *self, rvNavPath &goalPath, float &goalUtility ) {
	static const int MAX_ROUTED_CANDIDATES = 10;
	idList<botItemCandidate_t> candidates;
	const float searchRange = BOT_ITEM_SEARCH_RANGE * ( 0.5f + traits.itemFocus );
	const idVec3 origin = self->GetPhysics()->GetOrigin();

	goalPath.Clear();
	goalUtility = 0.0f;
	candidates.SetGranularity( MAX_ROUTED_CANDIDATES );

	const int startNode = navMesh.FindNearestNode( origin, 256.0f, true );
	if ( startNode == -1 ) {
		return NULL;
	}
	const int startArea = navMesh.GetNode( startNode ).area;

	for ( idEntity *ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		if ( !ent->IsType( idItem::GetClassType() ) || ent->GetInstance() != self->GetInstance() ) {
			continue;
		}

		idItem *item = static_cast<idItem *>( ent );
		if ( item->pickedUp || item->IsHidden() || IsAvoided( item ) ) {
			continue;
		}

		const float utility = ItemUtility( self, item );
		const float distance = ( item->GetPhysics()->GetOrigin() - origin ).Length();
		if ( utility <= 0.0f || distance > searchRange ) {
			continue;
		}

		const int itemNode = navMesh.FindNearestNode( item->GetPhysics()->GetOrigin(), 256.0f, true );
		if ( itemNode == -1 || navMesh.GetNode( itemNode ).area != startArea ) {
			continue;
		}

		botItemCandidate_t candidate;
		candidate.item = item;
		candidate.utility = utility;
		candidate.cheapScore = utility / ( distance + 192.0f );

		int insertAt = 0;
		while ( insertAt < candidates.Num() && candidates[insertAt].cheapScore >= candidate.cheapScore ) {
			insertAt++;
		}
		candidates.Insert( candidate, insertAt );
		if ( candidates.Num() > MAX_ROUTED_CANDIDATES ) {
			candidates.RemoveIndex( candidates.Num() - 1 );
		}
	}

	idEntity *best = NULL;
	float bestScore = 0.0f;
	for ( int i = 0; i < candidates.Num(); i++ ) {
		rvNavPath candidatePath;
		if ( !navMesh.FindPath( origin, candidates[i].item->GetPhysics()->GetOrigin(), candidatePath ) ) {
			continue;
		}

		const int claims = botManager.GoalClaimCount( this, candidates[i].item );
		const float claimScale = 1.0f + (float)claims * 1.5f;
		const float routeDistance = candidatePath.LengthFrom( origin );
		const float score = candidates[i].utility / ( ( routeDistance + 192.0f ) * claimScale );
		if ( score > bestScore ) {
			bestScore = score;
			best = candidates[i].item;
			goalPath = candidatePath;
			goalUtility = candidates[i].utility / claimScale;
		}
	}

	return best;
}

/*
================
rvBot::UpdateGoal
================
*/
void rvBot::UpdateGoal( idPlayer *self ) {
	const idVec3 origin = self->GetPhysics()->GetOrigin();
	idPlayer *currentFoe = enemy.GetEntity();
	const bool holdingObjective = AtObjectiveHoldPosition( self );

	// A reserved volume entry and an action already in flight are one movement.
	const bool actionActive = ( traversalCorner == pathCorner );
	const bool volumeEntryActive = traversalEntered && ( traversalCorner == pathCorner + 1 );
	if ( ( actionActive || volumeEntryActive ) && traversalCorner >= 0 &&
		 traversalCorner < path.Num() && path[traversalCorner].travelType != NAVTRAVEL_WALK ) {
		return;
	}

	if ( !path.IsEmpty() && pathCorner < path.Num() ) {
		const float remaining = PathDistanceRemaining( origin );
		if ( remaining + BOT_GOAL_PROGRESS_EPSILON < goalBestDistance ) {
			goalBestDistance = remaining;
			goalProgressTime = gameLocal.time;
			goalGiveUpTime = Max( goalGiveUpTime, gameLocal.time + BOT_RECOVER_IDLE_MSEC * 3 );
		}
	}

	if ( !holdingObjective && goalGiveUpTime && gameLocal.time > goalGiveUpTime &&
		 gameLocal.time - goalProgressTime > BOT_RECOVER_IDLE_MSEC ) {
		if ( bot_debug.GetInteger() >= 1 ) {
			gameLocal.Printf( "bot %s: giving up on goal %d at %s after no route progress\n",
				name.c_str(), goalType, goalOrigin.ToString( 0 ) );
		}
		AbandonGoal( BOT_GOAL_AVOID_MSEC );
	}

	// Preserve the useful item-denial reaction while validating the old goal.
	bool currentItemValid = false;
	if ( goalType == BOTGOAL_ITEM ) {
		idEntity *goal = goalEntity.GetEntity();
		idItem *item = ( goal && goal->IsType( idItem::GetClassType() ) ) ? static_cast<idItem *>( goal ) : NULL;
		currentItemValid = item && item->GetInstance() == self->GetInstance() &&
			!item->pickedUp && !item->IsHidden() && !path.IsEmpty();

		if ( item && item->pickedUp &&
			 ( item->GetPhysics()->GetOrigin() - origin ).LengthSqr() < BOT_ITEM_DENIED_RANGE * BOT_ITEM_DENIED_RANGE ) {
			idPlayer *thief = NULL;
			if ( item->pickedUpByClientNum >= 0 && item->pickedUpByClientNum != clientNum ) {
				idEntity *ent = gameLocal.entities[item->pickedUpByClientNum];
				if ( ent && ent->IsType( idPlayer::GetClassType() ) ) {
					thief = static_cast<idPlayer *>( ent );
				}
			}
			const idStr itemName = BotReadableName( item->spawnArgs );
			QueueChat( BOTCHAT_ITEM_DENIED, BotDisplayName( thief ), NULL, itemName.c_str() );
			Avoid( item, BOT_GOAL_AVOID_MSEC );
			goalEntity = NULL;
		}
	}

	const bool routeActive = !path.IsEmpty() && pathCorner < path.Num();
	if ( gameLocal.time < nextGoalSelectTime ) {
		// Moving targets still receive their inexpensive, throttled route refresh;
		// aim and fire continue independently whichever goal owns movement.
		if ( goalType == BOTGOAL_ENEMY && currentFoe &&
			 gameLocal.time - enemyPathTime > BOT_REPATH_COMBAT_MSEC ) {
			enemyPathTime = gameLocal.time;
			goalOrigin = EnemyPursuitOrigin();
			Repath( self, goalOrigin, true );
		} else if ( routeActive ) {
			return;
		}
		return;
	}
	nextGoalSelectTime = gameLocal.time + BOT_GOAL_SELECT_MSEC;

	botObjective_t objective;
	const bool hasObjective = BotFindObjective( self, objective );
	const bool objectiveAvoided = hasObjective && objective.entity.GetEntity() &&
		IsAvoided( objective.entity.GetEntity() );
	const bool objectiveAvailable = hasObjective && !objectiveAvoided;
	const bool enemyMovementAvailable = currentFoe && !IsAvoided( currentFoe );
	float objectivePriority = objectiveAvailable ? objective.priority : 0.0f;
	const idVec3 objectiveRouteOrigin = objectiveAvailable ?
		BotObjectiveRouteOrigin( self, objective ) : objective.origin;

	const bool currentObjectiveValid = goalType == BOTGOAL_OBJECTIVE && objectiveAvailable &&
		objectiveKind == objective.kind && goalEntity.GetEntity() == objective.entity.GetEntity();
	const bool currentObjectiveHeld = currentObjectiveValid && holdingObjective;
	const bool currentEnemyValid = goalType == BOTGOAL_ENEMY && enemyMovementAvailable &&
		goalEntity.GetEntity() == currentFoe && routeActive;
	const bool currentRoamValid = goalType == BOTGOAL_ROAM && routeActive;
	const bool currentValid = currentItemValid || currentObjectiveValid || currentEnemyValid || currentRoamValid;

	rvNavPath itemPath;
	float itemUtility = 0.0f;
	const bool wantHealth = WantsHealth( self );
	idEntity *item = NULL;
	if ( !objectiveAvailable || wantHealth || objectivePriority < 85.0f ) {
		item = PickItemGoal( self, itemPath, itemUtility );
	}
	float itemPriority = item ? 10.0f + itemUtility * 5.0f : 0.0f;
	const bool itemIsHealth = item && item->spawnArgs.GetInt( "inv_health", "0" ) > 0;
	const int maxHealth = Max( 1, self->inventory.maxHealth );
	const bool criticalHealth = self->health <= Max( 15, (int)( traits.retreatHealth * maxHealth * 0.65f ) );
	if ( itemIsHealth && wantHealth ) {
		itemPriority += criticalHealth ? 42.0f : 28.0f;
	}

	const bool foeVisible = currentFoe && gameLocal.time == enemyLastSeenTime;
	float enemyPriority = enemyMovementAvailable ?
		32.0f + traits.pursuit * 18.0f +
		traits.aggression * 8.0f + ( foeVisible ? 8.0f : 0.0f ) : 0.0f;
	if ( wantHealth ) {
		enemyPriority *= criticalHealth ? 0.35f : 0.65f;
	}
	if ( enemyMovementAvailable && currentFoe == lastAttacker.GetEntity() &&
		 gameLocal.time - lastAttackerTime < BOT_THREAT_MEMORY_MSEC ) {
		enemyPriority += 8.0f;
	}

	// Hysteresis compares the challenger with what the current goal is worth
	// now, not what it was worth when selected.  In particular, a new team claim
	// can deliberately lower an escort/rescue utility so another bot peels off.
	if ( currentObjectiveValid ) {
		goalUtility = objectivePriority;
	} else if ( currentEnemyValid ) {
		goalUtility = enemyPriority;
	}

	int desiredType = BOTGOAL_ROAM;
	float desiredPriority = 0.0f;
	if ( objectivePriority > desiredPriority ) {
		desiredType = BOTGOAL_OBJECTIVE;
		desiredPriority = objectivePriority;
	}
	if ( itemPriority > desiredPriority &&
		 ( objectivePriority < 100.0f || ( criticalHealth && self->health <= 15 ) ) ) {
		desiredType = BOTGOAL_ITEM;
		desiredPriority = itemPriority;
	}
	if ( enemyPriority > desiredPriority && objectivePriority <= 0.0f ) {
		desiredType = BOTGOAL_ENEMY;
		desiredPriority = enemyPriority;
	}

	const bool sameDesired =
		( desiredType == BOTGOAL_ITEM && goalType == desiredType && goalEntity.GetEntity() == item ) ||
		( desiredType == BOTGOAL_ENEMY && goalType == desiredType && goalEntity.GetEntity() == currentFoe ) ||
		( desiredType == BOTGOAL_OBJECTIVE && currentObjectiveValid );
	const bool urgentRuleChange = desiredType == BOTGOAL_OBJECTIVE &&
		( !currentObjectiveValid || desiredPriority >= 85.0f );

	if ( currentValid && !sameDesired && !urgentRuleChange &&
		 ( gameLocal.time < goalCommitUntil || desiredPriority < goalUtility * BOT_GOAL_SWITCH_MARGIN ) ) {
		return;
	}

	if ( sameDesired && currentObjectiveHeld ) {
		goalOrigin = objective.origin;
		goalUtility = desiredPriority;
		objectiveHoldPosition = objective.holdPosition;
		goalProgressTime = gameLocal.time;
		goalGiveUpTime = Max( goalGiveUpTime, gameLocal.time + BOT_GOAL_GIVEUP_MSEC );
		path.Clear();
		pathCorner = 0;
		pathTime = 0;
		repathFailures = 0;
		noRouteSince = 0;
		ResetTraversal();
		return;
	}

	if ( sameDesired && routeActive ) {
		goalUtility = desiredPriority;
		if ( desiredType == BOTGOAL_ENEMY && gameLocal.time - enemyPathTime > BOT_REPATH_COMBAT_MSEC ) {
			enemyPathTime = gameLocal.time;
			goalOrigin = EnemyPursuitOrigin();
			Repath( self, goalOrigin, true );
		} else if ( desiredType == BOTGOAL_OBJECTIVE ) {
			goalOrigin = objectiveRouteOrigin;
			idEntity *objectiveEntity = objective.entity.GetEntity();
			const bool movingObjective = objectiveEntity && objectiveEntity->IsType( idPlayer::GetClassType() );
			const int refresh = movingObjective ? BOT_REPATH_COMBAT_MSEC : BOT_REPATH_IDLE_MSEC;
			if ( gameLocal.time - pathTime > refresh ) {
				if ( !Repath( self, goalOrigin, true ) &&
					 ( goalOrigin - objective.origin ).LengthSqr() > 1.0f ) {
					goalOrigin = objective.origin;
					Repath( self, goalOrigin, true );
				}
			}
		} else if ( desiredType == BOTGOAL_ITEM && gameLocal.time - pathTime > BOT_REPATH_IDLE_MSEC ) {
			goalOrigin = item->GetPhysics()->GetOrigin();
			Repath( self, goalOrigin, true );
		}
		return;
	}

	bool routeFound = false;
	if ( desiredType == BOTGOAL_OBJECTIVE && objectiveAvailable ) {
		goalOrigin = objectiveRouteOrigin;
		routeFound = Repath( self, goalOrigin );
		if ( !routeFound && ( goalOrigin - objective.origin ).LengthSqr() > 1.0f ) {
			goalOrigin = objective.origin;
			routeFound = Repath( self, goalOrigin );
		}
		if ( routeFound ) {
			goalEntity = objective.entity.GetEntity();
			objectiveKind = objective.kind;
			objectiveHoldPosition = objective.holdPosition;
		}
	} else if ( desiredType == BOTGOAL_ITEM && item ) {
		path = itemPath;
		pathCorner = 0;
		pathTime = gameLocal.time;
		repathFailures = 0;
		noRouteSince = 0;
		ResetTraversal();
		routeFound = !path.IsEmpty();
		if ( routeFound ) {
			goalEntity = item;
			goalOrigin = item->GetPhysics()->GetOrigin();
		}
	} else if ( desiredType == BOTGOAL_ENEMY && currentFoe ) {
		enemyPathTime = gameLocal.time;
		goalOrigin = EnemyPursuitOrigin();
		routeFound = Repath( self, goalOrigin );
		if ( routeFound ) {
			goalEntity = currentFoe;
		}
	}

	if ( !routeFound && desiredType != BOTGOAL_ITEM && item && !itemPath.IsEmpty() ) {
		desiredType = BOTGOAL_ITEM;
		desiredPriority = itemPriority;
		path = itemPath;
		pathCorner = 0;
		pathTime = gameLocal.time;
		repathFailures = 0;
		noRouteSince = 0;
		ResetTraversal();
		routeFound = true;
		goalEntity = item;
		goalOrigin = item->GetPhysics()->GetOrigin();
	}
	if ( !routeFound && desiredType != BOTGOAL_ENEMY && currentFoe && enemyPriority > 0.0f ) {
		desiredType = BOTGOAL_ENEMY;
		desiredPriority = enemyPriority;
		enemyPathTime = gameLocal.time;
		goalOrigin = EnemyPursuitOrigin();
		routeFound = Repath( self, goalOrigin );
		if ( routeFound ) {
			goalEntity = currentFoe;
		}
	}

	if ( routeFound ) {
		goalType = desiredType;
		if ( desiredType != BOTGOAL_OBJECTIVE ) {
			objectiveKind = BOTOBJ_NONE;
			objectiveHoldPosition = false;
		}
		goalUtility = desiredPriority;
		goalCommitUntil = gameLocal.time + BOT_GOAL_COMMIT_MSEC;
		goalBestDistance = PathDistanceRemaining( origin );
		goalProgressTime = gameLocal.time;
		const int expectedTravel = (int)( goalBestDistance * ( 1000.0f / 180.0f ) );
		goalGiveUpTime = gameLocal.time + Max( BOT_GOAL_GIVEUP_MSEC, expectedTravel * 2 + 4000 );
		holdUntil = 0;
		stuckPathCorner = 0;
		stuckCornerDistance = idMath::INFINITY;
		return;
	}

	// A preferred candidate can be temporarily unreachable.  Preserve any old
	// valid route; otherwise fall back to a directed random wander.
	if ( currentValid && routeActive ) {
		return;
	}

	idVec3 roam;
	rvNavPath roamPath;
	if ( navMesh.RandomReachablePoint( origin, roam, &roamPath ) ) {
		goalType = BOTGOAL_ROAM;
		goalEntity = NULL;
		goalOrigin = roam;
		goalUtility = 1.0f;
		goalCommitUntil = gameLocal.time + BOT_GOAL_COMMIT_MSEC;
		goalGiveUpTime = gameLocal.time + BOT_GOAL_GIVEUP_MSEC;
		goalProgressTime = gameLocal.time;
		objectiveKind = BOTOBJ_NONE;
		objectiveHoldPosition = false;
		path = roamPath;
		pathCorner = 0;
		pathTime = gameLocal.time;
		repathFailures = 0;
		noRouteSince = 0;
		holdUntil = 0;
		stuckPathCorner = 0;
		stuckCornerDistance = idMath::INFINITY;
		ResetTraversal();
	}
}

/*
================
BotReachedTravelEnd

Special links join neighbouring samples, but a real jump keeps its momentum and
can land beyond that one-cell endpoint.  Accept the endpoint itself or a
bounded overshoot in the same corridor; never accept the takeoff side.
================
*/
static bool BotReachedTravelEnd( const idVec3 &origin, const idVec3 &start,
								 const idVec3 &end, float zTolerance ) {
	if ( idMath::Fabs( origin.z - end.z ) > zTolerance ) {
		return false;
	}

	idVec3 direction = end - start;
	direction.z = 0.0f;
	const float travelDistance = direction.Normalize();
	if ( travelDistance <= 0.0f ) {
		idVec3 remaining = end - origin;
		remaining.z = 0.0f;
		return remaining.LengthSqr() <= BOT_CORNER_REACH * BOT_CORNER_REACH;
	}

	// The ordinary corner radius is wider than one nav cell.  When both ends
	// share a floor, require a clear departure from the takeoff side before the
	// endpoint radius may win.  A real height change already proves that
	// departure and must accept a narrow-crate edge landing with little XY
	// progress.
	if ( idMath::Fabs( end.z - start.z ) <= zTolerance ) {
		idVec3 fromStart = origin - start;
		fromStart.z = 0.0f;
		const float minimumProgress = Min( travelDistance * 0.5f,
										   BOT_TRAVEL_ENTRY_REACH );
		if ( fromStart * direction < minimumProgress ) {
			return false;
		}
	}

	idVec3 remaining = end - origin;
	remaining.z = 0.0f;
	if ( remaining.LengthSqr() <= BOT_CORNER_REACH * BOT_CORNER_REACH ) {
		return true;
	}

	idVec3 past = origin - end;
	past.z = 0.0f;

	const float along = past * direction;
	if ( along < 0.0f || along > BOT_TRAVEL_OVERSHOOT ) {
		return false;
	}

	const idVec3 lateral = past - direction * along;
	return lateral.LengthSqr() <= BOT_CORNER_REACH * BOT_CORNER_REACH;
}

/*
================
BotTravelName
================
*/
static const char *BotTravelName( int travelType ) {
	switch ( travelType ) {
		case NAVTRAVEL_JUMP:		return "jump";
		case NAVTRAVEL_DROP:		return "drop";
		case NAVTRAVEL_JUMPPAD:	return "jump pad";
		case NAVTRAVEL_TELEPORT:	return "teleport";
		default:					return "travel";
	}
}

/*
================
BotTouchesTravelVolume

Pad and teleporter links retain their owning entity.  Expanding its live bounds
slightly lets the bot reserve the action before its hull actually activates the
volume, without guessing that every trigger has the same dimensions.
================
*/
static bool BotTouchesTravelVolume( idPlayer *self, const navCorner_t &corner ) {
	if ( corner.actionEntityNum < 0 || corner.actionEntityNum >= MAX_GENTITIES ) {
		return false;
	}

	idEntity *volume = gameLocal.entities[corner.actionEntityNum];
	if ( !volume ) {
		return false;
	}

	idBounds activationBounds;
	if ( volume->IsType( rvJumpPad::GetClassType() ) ) {
		if ( !static_cast<rvJumpPad *>( volume )->GetForceFieldBounds( activationBounds ) ) {
			return false;
		}
	} else {
		if ( !volume->GetPhysics() ) {
			return false;
		}
		activationBounds = volume->GetPhysics()->GetAbsBounds();
	}

	activationBounds.ExpandSelf( BOT_TRAVEL_ENTRY_REACH );

	return activationBounds.IntersectsBounds( self->GetPhysics()->GetAbsBounds() );
}

/*
================
rvBot::AdvancePath

Returns false once the route is exhausted.  Walk corners are proximity targets.
Every other travel type is an action with a source and destination: proximity
alone must never consume it, especially when one 24-unit nav cell is closer
than the ordinary 28-unit corner radius.
================
*/
bool rvBot::AdvancePath( idPlayer *self ) {
	const idVec3 origin = self->GetPhysics()->GetOrigin();

	while ( pathCorner < path.Num() ) {
		const navCorner_t &corner = path[pathCorner];
		idVec3 delta = corner.origin - origin;
		const float height = delta.z;
		delta.z = 0.0f;

		if ( corner.travelType == NAVTRAVEL_WALK ) {
			// The final walk corner before an action is its takeoff/entry point.
			// Reach it deliberately instead of consuming it and the adjacent
			// action destination in one pass.
			const bool actionEntry = ( pathCorner + 1 < path.Num() &&
									   path[pathCorner + 1].travelType != NAVTRAVEL_WALK );
			const int nextTravel = actionEntry ? path[pathCorner + 1].travelType : NAVTRAVEL_WALK;
			const bool volumeEntry = ( nextTravel == NAVTRAVEL_JUMPPAD ||
									   nextTravel == NAVTRAVEL_TELEPORT );

			if ( volumeEntry ) {
				const int actionCorner = pathCorner + 1;
				const navCorner_t &destination = path[actionCorner];

				// Do not consume a volume's source merely because the bot's
				// origin reached its centre.  Keep steering through the live
				// bounds until the pad launches it or the teleporter puts it on
				// the destination side.  This also catches an immediate
				// teleport that occurred between bot command frames.
				if ( traversalCorner != actionCorner ) {
					traversalCorner		= actionCorner;
					traversalStartOrigin	= corner.origin;
					traversalStartTime	= gameLocal.time;
					traversalEntered		= false;
					traversalStarted	= false;
				}

				const bool enteredVolume = BotTouchesTravelVolume( self, destination ) ||
					( destination.actionEntityNum < 0 &&
					  delta.LengthSqr() <= BOT_CORNER_REACH * BOT_CORNER_REACH &&
					  idMath::Fabs( height ) <= BOT_CORNER_REACH_Z );
				if ( enteredVolume && !traversalEntered ) {
					traversalEntered = true;
					traversalStartTime = gameLocal.time;
					unstickUntil	= 0;
					stuckChecks		= 0;
					stuckOrigin		= origin;
					stuckTime		= gameLocal.time;
				}

				const bool reachedExit = BotReachedTravelEnd( origin, corner.origin,
															  destination.origin,
															  BOT_CORNER_REACH_Z );
				const bool activated = reachedExit ||
					( nextTravel == NAVTRAVEL_JUMPPAD &&
					  traversalEntered && !self->pfl.onGround ) ||
					( nextTravel == NAVTRAVEL_TELEPORT && self->IsInTeleport() );

				if ( activated ) {
					pathCorner			= actionCorner;
					traversalStartTime	= gameLocal.time;
					traversalStarted	= true;
					unstickUntil		= 0;
					stuckChecks			= 0;
					stuckOrigin			= origin;
					stuckTime			= gameLocal.time;
					continue;
				}

				return true;
			}

			const float reach = actionEntry ? BOT_TRAVEL_ENTRY_REACH : BOT_CORNER_REACH;

			if ( delta.LengthSqr() > reach * reach ||
				 idMath::Fabs( height ) > BOT_CORNER_REACH_Z ) {
				return true;
			}

			pathCorner++;
			continue;
		}

		if ( traversalCorner != pathCorner ) {
			traversalCorner		= pathCorner;
			traversalStartOrigin	= origin;
			traversalStartTime	= gameLocal.time;
			traversalEntered		= false;
			traversalStarted	= false;

			// A previous generic recovery must not side-push a deliberate
			// takeoff, and its progress window starts here.
			unstickUntil	= 0;
			stuckChecks		= 0;
			stuckOrigin		= origin;
			stuckTime		= gameLocal.time;
		}

		if ( corner.travelType != NAVTRAVEL_TELEPORT && !self->pfl.onGround ) {
			traversalStarted = true;
		}

		// A teleporter may be vertical, may move less than one horizontal cell,
		// and may hold the player in a private camera before the exit.  Observe
		// its real state or its destination rather than guessing from XY
		// displacement.
		if ( corner.travelType == NAVTRAVEL_TELEPORT ) {
			if ( self->IsInTeleport() ||
				 BotReachedTravelEnd( origin, traversalStartOrigin,
									  corner.origin, BOT_CORNER_REACH_Z ) ) {
				traversalStarted = true;
			}
		}

		bool completed = false;
		switch ( corner.travelType ) {
			case NAVTRAVEL_JUMP:
			case NAVTRAVEL_DROP:
				// Same-height barriers make a Z-only test insufficient.  The
				// bot must first leave the ground, then land at or beyond the
				// destination on its sampled floor.
				completed = traversalStarted && self->pfl.onGround &&
							BotReachedTravelEnd( origin, traversalStartOrigin,
												 corner.origin, BOT_TRAVEL_REACH_Z );
				break;

			case NAVTRAVEL_JUMPPAD:
				// The authored target is the pad's flight target, not always the
				// exact place the player finally touches down.
				completed = traversalStarted &&
							BotReachedTravelEnd( origin, traversalStartOrigin,
												 corner.origin, BOT_CORNER_REACH_Z );
				break;

			case NAVTRAVEL_TELEPORT:
				completed = traversalStarted &&
							BotReachedTravelEnd( origin, traversalStartOrigin,
												 corner.origin, BOT_CORNER_REACH_Z );
				break;

			default:
				break;
		}

		if ( !completed ) {
			return true;
		}

		if ( bot_debug.GetInteger() >= 1 ) {
			gameLocal.Printf( "bot %s: completed %s from %s to %s at %s\n",
							  name.c_str(), BotTravelName( corner.travelType ),
							  traversalStartOrigin.ToString( 0 ),
							  corner.origin.ToString( 0 ), origin.ToString( 0 ) );
		}

		ResetTraversal();
		pathCorner++;
	}

	ResetTraversal();
	return false;
}

/*
================
rvBot::PressJump
================
*/
void rvBot::PressJump( usercmd_t &cmd ) const {
	// idPhysics_Player::CheckJump only fires on a fresh press: PMF_JUMP_HELD
	// blocks a held button and is cleared only by a released frame.  Pulse on
	// alternating grounded frames: this gives every attempt an immediate fresh
	// edge, releases on the next frame, and never sprays jump input throughout
	// the flight.  The client offset keeps a pack from launching in lockstep.
	idPlayer *self = GetPlayer();
	if ( self && self->pfl.onGround && ( ( gameLocal.framenum + clientNum ) & 1 ) == 0 ) {
		cmd.upmove = 127;
	}
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
rvBot::TrackDamage

The normal multiplayer damage path calls OnDamaged with an attacker.  This
timestamp edge remains as a fallback for hazards and mod damage paths that do
not have a player source.
================
*/
void rvBot::TrackDamage( idPlayer *self ) {
	if ( self->lastDmgTime == damageStamp ) {
		return;
	}

	damageStamp = self->lastDmgTime;
	ScheduleDodge( vec3_zero );
}

/*
================
rvBot::ScheduleDodge

A dodge owns explicit start/end times.  Repeated machinegun or lightning hits
therefore cannot keep moving the start line forward until the bot dies.
================
*/
void rvBot::ScheduleDodge( const idVec3 &threatDirection ) {
	if ( gameLocal.time < dodgeEndTime ) {
		return;
	}

	int delay = (int)traits.dodgeReactMsec;
	if ( MistakeActive( BOTMISTAKE_LATEDODGE ) ) {
		delay *= 2;
	}

	dodgeStartTime = gameLocal.time + Max( 0, delay );
	dodgeEndTime = dodgeStartTime + BOT_DODGE_MSEC;
	dodgeJump = ( gameLocal.random.RandomFloat() < traits.jumpChance );

	// Keep an existing useful side preference when the damage direction makes
	// one obvious; otherwise make the choice once for this dodge, not per frame.
	if ( threatDirection.LengthSqr() > idMath::FLOAT_EPSILON ) {
		idPlayer *self = GetPlayer();
		if ( self ) {
			idVec3 forward = aimAngles.ToForward();
			forward.z = 0.0f;
			idVec3 threat = threatDirection;
			threat.z = 0.0f;
			if ( forward.Normalize() > 0.0f && threat.Normalize() > 0.0f ) {
				strafeSide = ( forward.Cross( threat ).z < 0.0f ) ? -1.0f : 1.0f;
				return;
			}
		}
	}

	strafeSide = ( gameLocal.random.RandomFloat() < 0.5f ) ? -1.0f : 1.0f;
}

/*
================
rvBot::IsDodging
================
*/
bool rvBot::IsDodging( idPlayer *self ) const {
	return self && gameLocal.time >= dodgeStartTime && gameLocal.time < dodgeEndTime;
}

/*
================
BotMoveDirectionSafe

Combat steering is layered over a validated nav route.  Before accepting that
layer, sweep the real player bounds a short distance and make sure there is
walkable floor beyond it.  If it fails, the caller keeps the original route
direction instead of strafing off a ledge or into a wall.
================
*/
static bool BotMoveDirectionSafe( idPlayer *self, const idVec3 &direction ) {
	idVec3 dir = direction;
	dir.z = 0.0f;
	if ( dir.Normalize() <= 0.0f ) {
		return false;
	}

	const idVec3 start = self->GetPhysics()->GetOrigin();
	const idVec3 end = start + dir * 72.0f;
	trace_t sweep;
	gameLocal.TraceBounds( self, sweep, start, end, self->GetPhysics()->GetBounds(),
						   MASK_PLAYERSOLID, self );
	if ( sweep.fraction < 0.72f ) {
		return false;
	}

	trace_t floor;
	const idVec3 floorStart = sweep.endpos + idVec3( 0.0f, 0.0f, 12.0f );
	const idVec3 floorEnd = sweep.endpos - idVec3( 0.0f, 0.0f, 88.0f );
	gameLocal.TracePoint( self, floor, floorStart, floorEnd, MASK_PLAYERSOLID, self );

	return floor.fraction < 1.0f && floor.c.normal.z >= 0.55f;
}

/*
================
rvBot::UpdateMovement
================
*/
void rvBot::UpdateMovement( idPlayer *self, usercmd_t &cmd ) {
	const idVec3 origin = self->GetPhysics()->GetOrigin();

	TrackDamage( self );

	// Holding is an objective action, not an exhausted route.  Keep facing and
	// firing through the normal aim/weapon stages, but do not let generic combat
	// strafing, dodges, or off-mesh recovery walk the bot out of the scoring or
	// thaw volume it has already reached.
	if ( AtObjectiveHoldPosition( self ) ) {
		path.Clear();
		pathCorner = 0;
		pathTime = 0;
		repathFailures = 0;
		noRouteSince = 0;
		stuckChecks = 0;
		ResetTraversal();
		return;
	}

	// Look a short way down hostile projectile trajectories.  This is sampled,
	// not run every frame, and uses relative closest approach rather than a
	// simple radius so rockets travelling safely past do not cause panic.
	if ( gameLocal.time >= nextThreatScanTime ) {
		nextThreatScanTime = gameLocal.time + 80;

		idProjectile *threat = NULL;
		float timeToClosest = 0.0f;
		idVec3 closestPoint;
		if ( BotCombatFindIncomingProjectileThreat( self, 0.85f, 52.0f,
											 threat, timeToClosest, closestPoint ) ) {
			const bool alreadyDodging = gameLocal.time < dodgeEndTime;
			ScheduleDodge( threat->GetPhysics()->GetLinearVelocity() );
			if ( !alreadyDodging && !MistakeActive( BOTMISTAKE_LATEDODGE ) ) {
				// Skilled anticipation begins before impact, but never instantly:
				// dodgeReactMsec still contributes half its authored delay.
				const int predictiveDelay = Min( (int)( traits.dodgeReactMsec * 0.5f ),
											Max( 0, (int)( timeToClosest * 500.0f ) ) );
				dodgeStartTime = gameLocal.time + predictiveDelay;
				dodgeEndTime = dodgeStartTime + BOT_DODGE_MSEC;
			}
		}
	}

	if ( !AdvancePath( self ) ) {
		// Arrived, or never had a route.  UpdateGoal picks a new one next frame,
		// unless routing itself keeps failing - then the bot is off the mesh and
		// has to walk its own way back on.
		path.Clear();
		pathCorner	= 0;
		pathTime	= 0;
		ResetTraversal();
		if ( !noRouteSince ) {
			noRouteSince = gameLocal.time;
		}

		// Standing on an item that is still sitting there means it is not one
		// this bot can take - its own CTF flag, or a weapon it already has with
		// full ammo.  Routing to it again would loop forever.
		if ( goalType == BOTGOAL_ITEM ) {
			idEntity *goal = goalEntity.GetEntity();

			idItem *item = ( goal && goal->IsType( idItem::GetClassType() ) ) ? static_cast<idItem *>( goal ) : NULL;
			const bool stillThere = item && !item->pickedUp && !item->IsHidden();
			idVec3 itemDelta = stillThere ? goal->GetPhysics()->GetOrigin() - origin : vec3_zero;
			const float itemHeight = idMath::Fabs( itemDelta.z );
			itemDelta.z = 0.0f;

			if ( stillThere && self->pfl.onGround &&
				 itemHeight <= BOT_CORNER_REACH_Z &&
				 itemDelta.LengthSqr() < BOT_ITEM_ARRIVED * BOT_ITEM_ARRIVED ) {
				if ( bot_debug.GetInteger() >= 1 ) {
					gameLocal.Printf( "bot %s: '%s' did not pick up, looking elsewhere\n",
									  name.c_str(), goal->GetClassname() );
				}
				AbandonGoal( BOT_GOAL_AVOID_MSEC );
			}
		}

		// Reached the end of a wander with nothing to fight.  A patient bot
		// stands and watches the room for a moment instead of immediately
		// walking off to the next random point; that, and not a special camping
		// goal, is the whole of what an ambusher does that a roamer does not.
		//
		// Only patience ABOVE the baseline holds at all, so a bot with the
		// default 0.5 behaves exactly as it did before this existed.  The
		// wander-and-recover path below is load bearing - a stranded bot walks
		// its own way back onto the mesh through it - and a personality knob
		// must not be able to starve it, which is why being stranded
		// (repathFailures) cancels the hold outright.
		if ( goalType == BOTGOAL_ROAM && !enemy.GetEntity() && repathFailures < 2 ) {
			const int hold = (int)( Max( 0.0f, traits.patience - 0.5f ) * 2.0f * BOT_PATIENCE_HOLD_MSEC );

			if ( hold > 0 ) {
				if ( !holdUntil ) {
					holdUntil			= gameLocal.time + hold;
					nextGoalSelectTime	= Max( nextGoalSelectTime, holdUntil );
				}
				if ( gameLocal.time < holdUntil ) {
					// Standing still deliberately: no movement, and no recovery
					// either, because this bot is not lost.
					return;
				}
			}
		}

		holdUntil = 0;

		// Combat movement does not require a route.  This matters both on a tiny
		// platform where start and goal collapse to one node and while a failed
		// enemy path is cooling down.  Strafe relative to the enemy, not to an
		// arbitrary old route direction.
		idPlayer *foe = enemy.GetEntity();
		if ( foe && gameLocal.time == enemyLastSeenTime ) {
			idVec3 toFoe = foe->GetPhysics()->GetOrigin() - origin;
			toFoe.z = 0.0f;
			const float range = toFoe.Normalize();
			if ( range > 1.0f ) {
				float want = BotPreferredCombatRange( self, traits ) *
					( WantsHealth( self ) ? BOT_RETREAT_RANGE_SCALE : 1.0f );
				const float err = ( range - want ) / Max( want, 1.0f );
				const float willingness = ( err > 0.0f ) ?
					( 0.5f + traits.aggression ) : ( 1.5f - traits.aggression );
				const float radial = idMath::ClampFloat( -1.0f, 1.0f, err * 2.0f ) *
					traits.rangeDiscipline * willingness;
				const idVec3 combatBase = toFoe * radial;
				idVec3 side = toFoe.Cross( idVec3( 0.0f, 0.0f, 1.0f ) );
				float strafe = traits.strafeChance;
				const bool dodging = IsDodging( self );
				if ( dodging ) {
					strafe = Max( strafe, 0.9f );
				}
				idVec3 combatMove = combatBase;
				combatMove += side * ( strafeSide * strafe );
				combatMove.Normalize();
				bool combatMoveSafe = BotMoveDirectionSafe( self, combatMove );

				// A blocked dodge has two answers.  Try the opposite side before
				// surrendering movement entirely, and remember the side that worked
				// so the next frame does not test the obstruction first again.
				if ( !combatMoveSafe && dodging ) {
					idVec3 alternate = combatBase - side * ( strafeSide * strafe );
					alternate.Normalize();
					if ( BotMoveDirectionSafe( self, alternate ) ) {
						combatMove = alternate;
						combatMoveSafe = true;
						strafeSide = -strafeSide;
					}
				}

				if ( combatMoveSafe ) {
					ApplyMove( combatMove, cmd );
					if ( dodging && dodgeJump ) {
						PressJump( cmd );
					}
					return;
				}
			}
		}

		if ( repathFailures >= 2 || gameLocal.time - noRouteSince > BOT_RECOVER_IDLE_MSEC ) {
			RecoverToNavMesh( self, cmd );
		}
		return;
	}

	noRouteSince = 0;

	const navCorner_t &corner = path[pathCorner];
	const bool committedTravel = ( corner.travelType != NAVTRAVEL_WALK ) ||
		( traversalEntered && traversalCorner == pathCorner + 1 );

	idVec3 moveDir = corner.origin - origin;
	moveDir.z = 0.0f;
	const bool hasMoveDir = ( moveDir.Normalize() > 0.0f );
	const idVec3 routeDir = moveDir;

	bool wantDodgeJump = false;
	bool alternateDodgeValid = false;
	idVec3 alternateDodgeDir;

	// Style decides what fight the bot wants to be in.  combatRange is its
	// authored baseline, the active weapon pulls that toward a useful distance,
	// and rangeDiscipline is how hard it works at the result.  A rusher still
	// closes more eagerly, but no longer tries to use every gun at knife range.
	idPlayer *foe = enemy.GetEntity();

	if ( hasMoveDir && !committedTravel && foe &&
		 ( goalType == BOTGOAL_ENEMY || gameLocal.time == enemyLastSeenTime ) ) {
		idVec3 toFoe = foe->GetPhysics()->GetOrigin() - origin;
		toFoe.z = 0.0f;

		const float range = toFoe.Normalize();

		// Looking for health means wanting to be further away than usual, which
		// is a change of distance rather than of behaviour: the bot still
		// fights back while it withdraws.
		float want = BotPreferredCombatRange( self, traits );
		if ( WantsHealth( self ) ) {
			want *= BOT_RETREAT_RANGE_SCALE;
		}

		if ( range > 1.0f && want > 1.0f ) {
			const float err = ( range - want ) / want;

			// aggression is the asymmetry, not the strength: a pushy bot closes
			// hard and is reluctant to give ground, a cautious one the reverse.
			// The strength is rangeDiscipline, which is competence.
			const float willingness	= ( err > 0.0f ) ? ( 0.5f + traits.aggression ) : ( 1.5f - traits.aggression );
			const float push		= idMath::ClampFloat( -1.0f, 1.0f, err * 2.0f ) * traits.rangeDiscipline * willingness;

			moveDir += toFoe * push;

			// Sidestep so the bot is not a straight line target.  Inside the
			// fighting band it always dodges a little; once it is actually
			// being hit it dodges properly, after dodgeReactMsec.
			if ( range < want * BOT_STRAFE_RANGE_SCALE ) {
				if ( gameLocal.time >= strafeFlipTime ) {
					const int rhythm = Max( 1, (int)traits.strafeRhythmMsec );
					const int variance = Max( 1, (int)traits.strafeRhythmVarianceMsec );

					strafeFlipTime	= gameLocal.time + rhythm + gameLocal.random.RandomInt( variance );
					strafeSide		= -strafeSide;
				}

				float strafe = traits.strafeChance;
				const bool dodging = IsDodging( self );

				if ( dodging ) {
					strafe			= Max( strafe, 0.85f );
					wantDodgeJump	= dodgeJump;
				}

				idVec3 side = toFoe.Cross( idVec3( 0.0f, 0.0f, 1.0f ) );
				const idVec3 strafeOffset = side * ( strafeSide * strafe );
				if ( dodging ) {
					alternateDodgeDir = moveDir - strafeOffset;
					alternateDodgeValid = alternateDodgeDir.Normalize() > 0.0f;
				}
				moveDir += strafeOffset;
			}

			moveDir.Normalize();
		}
	}

	// Local separation keeps nearby players from becoming one collision blob at
	// a doorway or item.  It is a steering hint only and is suppressed for the
	// precise entry to a jump, pad, drop or teleporter.
	if ( hasMoveDir && !committedTravel ) {
		for ( int i = 0; i < gameLocal.numClients; i++ ) {
			idPlayer *other = gameLocal.GetClientByNum( i );
			if ( !other || other == self || other->spectating || other->health <= 0 ||
				 other->GetInstance() != self->GetInstance() ) {
				continue;
			}

			idVec3 away = origin - other->GetPhysics()->GetOrigin();
			away.z = 0.0f;
			const float distance = away.Normalize();
			if ( distance > 1.0f && distance < 96.0f ) {
				const idVec3 separation = away * ( ( 96.0f - distance ) / 96.0f ) * 0.75f;
				moveDir += separation;
				if ( alternateDodgeValid ) {
					alternateDodgeDir += separation;
				}
			}
		}
		moveDir.Normalize();
		if ( alternateDodgeValid ) {
			alternateDodgeValid = alternateDodgeDir.Normalize() > 0.0f;
		}
	}

	if ( hasMoveDir && !committedTravel && gameLocal.time < unstickUntil ) {
		// Push sideways out of whatever the bot walked into.
		idVec3 side = moveDir.Cross( idVec3( 0.0f, 0.0f, 1.0f ) );
		moveDir += side * unstickSide;
		moveDir.Normalize();
		if ( alternateDodgeValid ) {
			alternateDodgeDir += side * unstickSide;
			alternateDodgeValid = alternateDodgeDir.Normalize() > 0.0f;
		}
	}

	// The nav direction is already floor-validated.  Any combat, separation or
	// recovery alteration must earn the same confidence before replacing it.
	if ( hasMoveDir && !committedTravel &&
		 ( moveDir - routeDir ).LengthSqr() > 0.01f && !BotMoveDirectionSafe( self, moveDir ) ) {
		if ( alternateDodgeValid && BotMoveDirectionSafe( self, alternateDodgeDir ) ) {
			moveDir = alternateDodgeDir;
			strafeSide = -strafeSide;
		} else {
			moveDir = routeDir;
			wantDodgeJump = false;
		}
	}

	if ( hasMoveDir ) {
		ApplyMove( moveDir, cmd );
	}

	// A special link owns its input until its exit.  Ordinary walking may still
	// use the generic obstruction and combat-dodge jumps, but those must not
	// perturb drops, jump pads, or teleport entries.
	const bool wantJump = ( corner.travelType == NAVTRAVEL_JUMP ) ||
						  ( !committedTravel && gameLocal.time < unstickUntil ) ||
						  ( !committedTravel && wantDodgeJump );

	if ( wantJump ) {
		PressJump( cmd );
	}

	// Stuck detection is horizontal: jumping in place must not count as route
	// progress.  Give a special traversal time to complete, then require two
	// consecutive failed samples before discarding its route.
	if ( gameLocal.time - stuckTime > BOT_STUCK_CHECK_MSEC ) {
		const bool actionCurrent = ( traversalCorner == pathCorner &&
									 corner.travelType != NAVTRAVEL_WALK );
		const int traversalMsec = gameLocal.time - traversalStartTime;
		const bool transportInProgress = actionCurrent &&
			( ( corner.travelType == NAVTRAVEL_JUMPPAD && !self->pfl.onGround ) ||
			  ( corner.travelType == NAVTRAVEL_TELEPORT && self->IsInTeleport() ) );
		const bool traversalTimedOut = actionCurrent &&
									  traversalMsec > BOT_TRAVEL_TIMEOUT_MSEC;
		const bool traversalGrace = committedTravel &&
								   ( traversalCorner == pathCorner ||
									 ( traversalEntered && traversalCorner == pathCorner + 1 ) ) &&
								   traversalMsec <= BOT_TRAVEL_COMMIT_MSEC;
		idVec3 cornerDelta = corner.origin - origin;
		cornerDelta.z = 0.0f;
		const float cornerDistance = cornerDelta.Length();
		const bool routeProgress = pathCorner > stuckPathCorner ||
			( pathCorner == stuckPathCorner &&
			  cornerDistance + BOT_GOAL_PROGRESS_EPSILON < stuckCornerDistance );

		if ( traversalTimedOut ) {
			path.Clear();
			pathTime = 0;
			ResetTraversal();
			stuckChecks = 0;

			if ( bot_debug.GetBool() ) {
				gameLocal.Printf( "bot %s: traversal timed out at %s\n",
								  name.c_str(), origin.ToString( 0 ) );
			}
		} else if ( transportInProgress || traversalGrace ) {
			stuckChecks = 0;
		} else if ( !routeProgress ) {
			stuckChecks++;
			unstickUntil	= gameLocal.time + BOT_UNSTICK_MSEC;
			unstickSide		= ( gameLocal.random.RandomFloat() < 0.5f ) ? -1.0f : 1.0f;

			// Two failures in a row means the route itself is bad.
			if ( stuckChecks >= 2 && path.Num() > 0 ) {
				path.Clear();
				pathTime = 0;
				ResetTraversal();
				stuckChecks = 0;

				if ( bot_debug.GetBool() ) {
					gameLocal.Printf( "bot %s: stuck at %s\n", name.c_str(), origin.ToString( 0 ) );
				}
			}
		} else {
			stuckChecks = 0;
		}

		stuckOrigin	= origin;
		stuckTime	= gameLocal.time;
		stuckPathCorner = pathCorner;
		stuckCornerDistance = cornerDistance;
	}
}

/*
================
rvBot::ProjectileSpeed

Forward launch speed of whatever is in the bot's hands, including arcing
projectiles; UpdateAim applies their gravity separately.
================
*/
float rvBot::ProjectileSpeed( idPlayer *self ) const {
	if ( !self->weapon ) {
		return 0.0f;
	}

	// The test is the presence of def_projectile, NOT rvWeapon::wfl.
	// attackHitscan.  The gauntlet and the lightning gun set neither
	// def_projectile nor def_hitscan and trace for themselves, so attackHitscan
	// is false for both even though both are instant hit - a bot that led with
	// a lightning gun would miss with it every time.  rvWeapon::spawnArgs
	// rather than the weapon decl, because that is the only view of the keys
	// with the weapon mods applied.
	const char *projName = self->weapon->spawnArgs.GetString( "def_projectile", "" );
	if ( !projName[0] ) {
		return 0.0f;
	}

	// Resolved through gameLocal so the multiplayer retune applies:
	// idGameLocal::FindEntityDef silently prefers <name>_mp in multiplayer and
	// every MP projectile is faster than its single player original - rocket
	// 935 not 900, nail 1400 not 1200.  A hardcoded table would lead short on
	// every shot.
	const idDict *dict = gameLocal.FindEntityDefDict( projName, false );
	if ( !dict ) {
		return 0.0f;
	}

	// Multiplayer forbids speed ramps outright - idProjectile::Launch asserts
	// !gameLocal.isServer when speed_end is present - so this single constant
	// is the whole flight model.
	return idProjectile::GetVelocity( dict ).x;
}

/*
================
rvBot::ProjectileGravity
================
*/
idVec3 rvBot::ProjectileGravity( idPlayer *self ) const {
	if ( !self->weapon ) {
		return vec3_zero;
	}

	const char *projName = self->weapon->spawnArgs.GetString( "def_projectile", "" );
	if ( !projName[0] ) {
		return vec3_zero;
	}
	const idDict *dict = gameLocal.FindEntityDefDict( projName, false );
	return dict ? idProjectile::GetGravity( dict ) : vec3_zero;
}

/*
================
rvBot::UpdateAim

The bot aims at where it BELIEVES the target is, which trails the truth; the
belief is led for projectile flight, offset by the target's angular rate, and
shaken by a continuous tremor; and the view chases that with a damped
second-order slew rather than a straight clamp, which is what produces the
overshoot and hunting at low skill and the clean settle at high skill.
================
*/
void rvBot::UpdateAim( idPlayer *self, usercmd_t &cmd ) {
	idVec3	eye;
	idMat3	axis;

	self->GetViewPos( eye, axis );

	// A frame hitch must not teleport the view: everything below integrates.
	const float	dt	= idMath::ClampFloat( 0.001f, 0.1f, MS2SEC( gameLocal.GetMSec() ) );
	idPlayer *	foe	= enemy.GetEntity();

	aimPointValid = false;

	if ( foe ) {
		const idBounds &bounds = foe->GetPhysics()->GetAbsBounds();

		idVec3 truePos = bounds.GetCenter();
		const float heightBias = BOT_AIM_HEIGHT_BIAS * ( traits.weaponSkill * 2.0f - 1.0f ) + traits.aimHeight;

		truePos.z = idMath::ClampFloat( bounds[0].z + 1.0f, bounds[1].z - 1.0f,
									   truePos.z + ( bounds[1].z - bounds[0].z ) * heightBias );

		// The belief only follows what the bot can actually see.  An enemy is
		// remembered for a while after it breaks line of sight, and a belief
		// that kept updating through the wall would have the bot tracking you
		// perfectly while you were behind it and already on target the instant
		// you stepped out.  Out of sight, the aim coasts on the last position
		// it had - which is exactly what the word believed means.
		const bool foeVisible = ( gameLocal.time == enemyLastSeenTime );
		if ( foeVisible ) {
			// Use the exposed chest/head/pelvis sample found by perception rather
			// than aiming back into the covered centre of the model.  Keep a
			// reduced character/skill height bias within the exposed body.
			truePos = enemyVisiblePoint;
			truePos.z = idMath::ClampFloat( bounds[0].z + 1.0f, bounds[1].z - 1.0f,
				truePos.z + ( bounds[1].z - bounds[0].z ) * heightBias * 0.35f );
		}

		// First order lag.  aimTrackTimeConst seconds after the target changes
		// direction the bot is still aiming most of the way back where it was.
		if ( !aimBeliefValid ) {
			aimBelief		= truePos;
			aimBeliefValid	= true;
		} else if ( foeVisible && !MistakeActive( BOTMISTAKE_LOSETRACK ) ) {
			const float alpha = 1.0f - idMath::Exp( -dt / Max( traits.aimTrackTimeConst, 0.01f ) );

			aimBelief += ( truePos - aimBelief ) * alpha;
		} else if ( !MistakeActive( BOTMISTAKE_LOSETRACK ) ) {
			// Briefly extrapolate the last observed velocity, with confidence
			// fading to zero.  This predicts a doorway exit without tracking live
			// physics through the wall.
			const float unseen = MS2SEC( gameLocal.time - enemyLastSeenTime );
			const float confidence = idMath::ClampFloat( 0.0f, 1.0f, 1.0f - unseen / 0.75f );
			aimBelief += enemyLastSeenVelocity * ( dt * traits.pursuit * confidence );
		}

		aimPoint = aimBelief;

		const float speed = ProjectileSpeed( self );

		if ( speed > 0.0f ) {
			const idVec3 foeVel = foeVisible ? foe->GetPhysics()->GetLinearVelocity() : enemyLastSeenVelocity;
			const idVec3 ownVel = self->GetPhysics()->GetLinearVelocity();
			const idVec3 gravity = ProjectileGravity( self );

			// Compensating for its own motion is the same skill as leading the
			// target, so it rides the same knob rather than a second one.
			const idVec3 relVel = foeVel - ownVel * ( 1.0f - idMath::ClampFloat( 0.0f, 1.0f, traits.aimLead ) );

			// Two fixed point iterations is plenty: the flight time changes
			// very little once the lead is roughly right.
			idVec3	lead;
			idVec3	gravityCompensation;
			float	flight = ( aimBelief - eye ).Length() / speed;

			for ( int i = 0; i < 3; i++ ) {
				lead	= relVel * ( flight * traits.aimLead * aimLeadRoll );
				gravityCompensation = gravity * ( -0.5f * flight * flight * traits.aimLead );
				flight	= ( aimBelief + lead + gravityCompensation - eye ).Length() / speed;
			}

			aimPoint = aimBelief + lead + gravityCompensation;
		}

		aimPointValid = true;
	} else if ( pathCorner < path.Num() ) {
		aimPoint = path[pathCorner].origin;
		aimPoint.z += pm_normalheight.GetFloat() * 0.5f;
	} else {
		aimPoint = eye + aimAngles.ToForward() * 256.0f;
	}

	idVec3 dir = aimPoint - eye;
	if ( dir.Normalize() <= 0.0f ) {
		// Leave the view where it is.  rvBot::Think carries last frame's
		// cmd.angles across its memset for exactly this case, and the player
		// re-latches its delta from them, so the view holds instead of snapping.
		return;
	}

	idAngles pointAngles = dir.ToAngles();
	pointAngles.roll = 0.0f;

	// Tracking error: a lag ACROSS the view rather than noise around it.  The
	// faster the target crosses, the further behind the bot's aim sits, which
	// is why a strafing player is harder for a bot to hit than a charging one.
	float errPitch	= 0.0f;
	float errYaw	= 0.0f;

	if ( aimTrackValid ) {
		const float dp	= idMath::AngleDelta( pointAngles.pitch, aimTrackAngles.pitch );
		const float dy	= idMath::AngleDelta( pointAngles.yaw, aimTrackAngles.yaw );
		const float mag	= idMath::Sqrt( dp * dp + dy * dy );

		aimAngVel += ( mag / dt - aimAngVel ) * BOT_ANGVEL_FILTER;

		if ( foe && mag > idMath::FLOAT_EPSILON ) {
			const float lag = traits.aimTrackError * aimAngVel * 0.01f;

			errPitch	= -( dp / mag ) * lag;
			errYaw		= -( dy / mag ) * lag;
		}
	}

	aimTrackAngles	= pointAngles;
	aimTrackValid	= true;

	// Tremor: continuous and smooth, so it reads as a hand rather than as the
	// white noise a re-rolled step offset produced.  Two octaves, four phases,
	// all seeded from the client number.
	const float t			= MS2SEC( gameLocal.time );
	const float w			= idMath::TWO_PI * traits.aimTremorRateHz;
	const float tremorPitch	= traits.aimTremorDeg * ( 0.65f * idMath::Sin( w * t + tremorPhase[0] ) +
													  0.35f * idMath::Sin( BOT_TREMOR_OCTAVE * w * t + tremorPhase[1] ) );
	const float tremorYaw	= traits.aimTremorDeg * ( 0.65f * idMath::Sin( w * t + tremorPhase[2] ) +
													  0.35f * idMath::Sin( BOT_TREMOR_OCTAVE * w * t + tremorPhase[3] ) );

	idAngles desired = pointAngles;

	desired.pitch	+= errPitch + tremorPitch;
	desired.yaw		+= errYaw + tremorYaw;

	// Damped second order slew.  turnAccel over turnSpeed is the natural
	// frequency, which keeps the two knobs consistent: raising the cap without
	// raising the acceleration makes a bot that is quick but sloppy.
	float wn = traits.turnAccel / Max( traits.turnSpeed, 1.0f );

	// See BOT_TURN_STABLE_WN: content can ask for a frequency this explicit
	// integrator cannot represent, and the failure mode is a spinning view.
	wn = Min( wn, BOT_TURN_STABLE_WN / dt );

	for ( int i = 0; i < 2; i++ ) {
		const float err		= idMath::AngleDelta( desired[i], aimAngles[i] );
		const float accel	= wn * wn * err - 2.0f * traits.turnDamping * wn * aimRate[i];

		aimRate[i]		= idMath::ClampFloat( -traits.turnSpeed, traits.turnSpeed, aimRate[i] + accel * dt );
		aimAngles[i]	= idMath::AngleNormalize180( aimAngles[i] + aimRate[i] * dt );
	}

	// The pitch stop has to kill the rate as well, or the slew winds up against
	// it and then unwinds all at once when the target moves back into range.
	if ( aimAngles.pitch > 85.0f || aimAngles.pitch < -85.0f ) {
		aimAngles.pitch	= idMath::ClampFloat( -85.0f, 85.0f, aimAngles.pitch );
		aimRate.pitch	= 0.0f;
	}

	aimAngles.roll	= 0.0f;
	aimRate.roll	= 0.0f;

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
	nextWeaponTime = gameLocal.time + Max( 1, (int)traits.weaponSwitchMsec );

	// A wrong-weapon mistake is not a wrong choice, it is no choice: the bot
	// keeps whatever is already in its hands until it notices.
	if ( MistakeActive( BOTMISTAKE_WRONGWEAPON ) ) {
		return;
	}

	idPlayer *	foe		= enemy.GetEntity();
	float		range	= BOT_WEAPON_RANGE_SPLIT * 2.0f;

	if ( foe ) {
		range = ( foe->GetPhysics()->GetOrigin() - self->GetPhysics()->GetOrigin() ).Length();
	}

	// weaponSkill is the chance the range-correct choice gets made at all.  A
	// poor bot takes the first thing it can shoot with, which is exactly what
	// the ordered profile table gives it.
	const bool careful = ( gameLocal.random.RandomFloat() < traits.weaponSkill );

	int		bestSlot	= -1;
	float	bestScore	= 0.0f;
	int		firstSlot	= -1;
	float	currentScore = 0.0f;
	const int currentSlot = self->GetCurrentWeapon();

	float rangeBlend = idMath::ClampFloat( 0.0f, 1.0f,
		( range - BOT_WEAPON_RANGE_SPLIT * 0.5f ) / ( BOT_WEAPON_RANGE_SPLIT * 1.5f ) );
	rangeBlend = rangeBlend * rangeBlend * ( 3.0f - 2.0f * rangeBlend );

	for ( int i = 0; i < BOT_NUM_WEAPON_RANGES; i++ ) {
		const char *weaponName = botWeaponRanges[i].name;
		const int slot = self->GetWeaponIndex( weaponName );

		// GetWeaponIndex answers 0 for anything it does not know, so confirm
		// the slot really is the weapon that was asked for.
		if ( idStr::Icmp( self->spawnArgs.GetString( va( "def_weapon%d", slot ), "" ), weaponName ) != 0 ) {
			continue;
		}
		if ( !( self->inventory.weapons & ( 1 << slot ) ) ) {
			continue;
		}
		if ( !self->inventory.HasAmmo( weaponName ) ) {
			continue;
		}

		if ( firstSlot == -1 ) {
			firstSlot = slot;
		}

		float suitability = botWeaponRanges[i].closeScore +
			( botWeaponRanges[i].farScore - botWeaponRanges[i].closeScore ) * rangeBlend;

		// Splash at handshake distance is a liability; the final fire trace is a
		// second line of defence, but choosing a safer gun is better than holding
		// a rocket and declining every shot.
		if ( range < 150.0f &&
			 ( !idStr::Icmp( weaponName, "weapon_rocketlauncher" ) ||
			   !idStr::Icmp( weaponName, "weapon_grenadelauncher" ) ||
			   !idStr::Icmp( weaponName, "weapon_napalmgun" ) ) ) {
			suitability *= 0.2f;
		}
		if ( range < 96.0f && !idStr::Icmp( weaponName, "weapon_gauntlet" ) ) {
			suitability = 14.0f;
		}

		const float score = suitability * rvBotCharacterManager::WeaponBias( traits, weaponName );
		if ( slot == currentSlot ) {
			currentScore = score;
		}

		if ( score > bestScore ) {
			bestScore	= score;
			bestSlot	= slot;
		}
	}

	int want = bestSlot;
	if ( !careful && currentScore > 0.0f ) {
		// The mistake is inertia, not a deterministic preference for whichever
		// weapon happens to be first in the table.
		want = currentSlot;
	} else if ( want == -1 ) {
		want = firstSlot;
	}
	if ( want == -1 ) {
		return;
	}

	if ( want != currentSlot ) {
		// Minimum dwell and a score margin stop range-boundary oscillation.  An
		// empty/invalid current weapon has currentScore zero and switches at once.
		if ( currentScore > 0.0f &&
			 ( gameLocal.time - lastWeaponSwitchTime < 850 || bestScore < currentScore * 1.18f ) ) {
			return;
		}
		self->SelectWeapon( want, false );
		lastWeaponSwitchTime = gameLocal.time;
	}
}

/*
================
rvBot::BurstAllows

Trigger discipline for automatic weapons: a burst, then a pause long enough for
the aim to re-settle.  Low skill holds the trigger down for over a second at a
time and hits nothing with most of it; high skill taps.
================
*/
bool rvBot::BurstAllows( idPlayer *self ) {
	// Anything that fires one shot per press has its own rhythm already and a
	// burst window would only stutter it.  For a rail gun the settle time is
	// the whole of the discipline.
	const float fireRate = self->weapon ? self->weapon->spawnArgs.GetFloat( "fireRate", "0" ) : 0.0f;

	if ( fireRate <= 0.0f || fireRate > BOT_AUTO_FIRE_RATE ) {
		return true;
	}

	if ( gameLocal.time < burstEndTime ) {
		return true;
	}
	if ( gameLocal.time < burstRestTime ) {
		onTargetTime = 0;
		return false;
	}

	const int spread	= Max( 1, (int)( traits.burstMaxMsec - traits.burstMinMsec ) );
	const int length	= (int)traits.burstMinMsec + gameLocal.random.RandomInt( spread );

	burstEndTime	= gameLocal.time + length;
	burstRestTime	= burstEndTime + (int)traits.burstPauseMsec;

	// The lead error is rolled per burst, not per frame.  A shot that missed in
	// front should keep missing in front until the bot lets go of the trigger
	// and takes another look; re-rolling every frame would average out to a
	// perfect lead over any burst long enough to matter.
	aimLeadRoll = 1.0f + gameLocal.random.CRandomFloat() * traits.aimLeadError;

	return true;
}

/*
================
BotWeaponSplashRadius

Reads the live, MP-resolved projectile and damage definitions.  Weapon mods and
the shipped multiplayer retune therefore feed the same safety radius used by
the actual explosion.
================
*/
static float BotWeaponSplashRadius( idPlayer *self ) {
	if ( !self || !self->weapon ) {
		return 0.0f;
	}

	const char *projectileName = self->weapon->spawnArgs.GetString( "def_projectile", "" );
	const idDict *projectile = projectileName[0] ? gameLocal.FindEntityDefDict( projectileName, false ) : NULL;
	if ( !projectile ) {
		return 0.0f;
	}

	const char *splashName = projectile->GetString( "def_splash_damage", "" );
	const idDict *splash = splashName[0] ? gameLocal.FindEntityDefDict( splashName, false ) : NULL;
	return splash ? Max( 0.0f, splash->GetFloat( "radius", "0" ) ) : 0.0f;
}

/*
================
rvBot::UpdateFire
================
*/
void rvBot::UpdateFire( idPlayer *self, usercmd_t &cmd ) {
	idPlayer *foe = enemy.GetEntity();

	if ( !foe || !aimPointValid || !IsEnemy( self, foe ) ) {
		onTargetTime	= 0;
		wasFiring		= false;
		return;
	}

	// The reaction deadline was computed once, when the target registered.
	if ( gameLocal.time < enemyFireTime ) {
		onTargetTime	= 0;
		wasFiring		= false;
		return;
	}

	// Most personalities only shoot at what they can see.  A suppressive one
	// keeps the trigger down briefly at the last believed position, putting
	// fire through the doorway instead of becoming silent the instant contact
	// crosses it.
	const int unseenMsec = gameLocal.time - enemyLastSeenTime;
	const bool suppressing = enemyLastSeenTime > 0 &&
							 unseenMsec > 0 &&
							 unseenMsec <= (int)traits.suppressionMsec;

	if ( gameLocal.time != enemyLastSeenTime && !suppressing ) {
		onTargetTime	= 0;
		wasFiring		= false;
		return;
	}
	if ( !self->weapon || self->weapon->IsReloading() || self->weapon->IsHolstered() ) {
		onTargetTime = 0;
		wasFiring = false;
		return;
	}

	idVec3	eye;
	idMat3	axis;

	self->GetViewPos( eye, axis );

	// Measured against the aim POINT, not the enemy's centre.  If the two
	// disagreed the bot would aim at where it is leading and then refuse to
	// pull the trigger because the centre of the target was outside the cone.
	idVec3 dir = aimPoint - eye;
	if ( dir.Normalize() <= 0.0f ) {
		return;
	}

	const float angle			= RAD2DEG( idMath::ACos( idMath::ClampFloat( -1.0f, 1.0f, dir * aimAngles.ToForward() ) ) );
	const float turnRate		= idMath::Sqrt( aimRate.pitch * aimRate.pitch + aimRate.yaw * aimRate.yaw );
	const float fireCone		= traits.fireConeDeg * ( 0.5f + traits.initiative );
	const float holdTurnRate	= traits.holdFireTurnRate * ( 0.5f + traits.initiative );
	const int settleMsec		= (int)( traits.aimSettleMsec * ( 2.0f - traits.initiative * 2.0f ) );

	// Two gates, and the second one matters as much as the first: a view still
	// swinging hard is not aimed at anything, it is passing over it.
	if ( angle > fireCone || turnRate > holdTurnRate ) {
		onTargetTime	= 0;
		wasFiring		= false;
		return;
	}

	onTargetTime += gameLocal.GetMSec();

	// A mistimed shot is the bot pulling the trigger before the sight has
	// stopped moving, which is what a rattled player does.
	const bool settled = ( onTargetTime >= settleMsec ) || MistakeActive( BOTMISTAKE_MISTIMEDSHOT );

	if ( !settled || !BurstAllows( self ) ) {
		wasFiring = false;
		return;
	}

	const float fireRate = self->weapon->spawnArgs.GetFloat( "fireRate", "0" );
	const bool singleShot = ( fireRate <= 0.0f || fireRate > BOT_AUTO_FIRE_RATE );
	if ( singleShot && ( gameLocal.time < nextSingleShotTime || !self->weapon->IsReady() ) ) {
		wasFiring = false;
		return;
	}

	const float splashRadius = BotWeaponSplashRadius( self );
	if ( !BotCombatLineOfFireIsSafe( self, foe, eye, aimPoint, splashRadius,
										 !suppressing ) ) {
		wasFiring = false;
		return;
	}

	cmd.buttons |= BUTTON_ATTACK;

	if ( singleShot ) {
		const int cadence = Max( gameLocal.GetMSec(), (int)( fireRate * 1000.0f * 0.85f ) );
		nextSingleShotTime = gameLocal.time + cadence;
	}

	if ( bot_debugAim.GetInteger() >= 1 && ( !wasFiring || bot_debugAim.GetInteger() >= 2 ) ) {
		if ( bot_debugAim.GetInteger() < 2 || gameLocal.time >= nextAimDebugTime ) {
			nextAimDebugTime = gameLocal.time + 250;

			gameLocal.Printf( "bot %s: %s at %s, cone %.1f/%.1f deg, slew %.0f/%.0f deg/s, settled %d/%d ms, lead x%.2f\n",
							  name.c_str(), suppressing ? "suppress" : "fire",
							  BotDisplayName( foe ),
							  angle, fireCone,
							  turnRate, holdTurnRate,
							  onTargetTime, settleMsec,
							  aimLeadRoll );
		}
	}
	if ( singleShot ) {
		onTargetTime = 0;
		aimLeadRoll = 1.0f + gameLocal.random.CRandomFloat() * traits.aimLeadError;
	}

	wasFiring = true;
}

/*
================
rvBot::QueueChat

Chat is queued, never sent from the event.  A bot that answers a frag on the
frame it landed reads as a script, and a worse player takes longer to type.
================
*/
void rvBot::QueueChat( rvBotChatEvent event, const char *other, const char *weapon, const char *item ) {
	if ( !active || !character ) {
		return;
	}
	if ( !gameLocal.isMultiplayer || gameLocal.isClient || !gameLocal.isServer ) {
		return;
	}
	if ( bot_chat.GetInteger() <= 0 || !bot_characters.GetBool() ) {
		return;
	}

	float chatiness = traits.chatiness;

	if ( bot_chat.GetInteger() >= 2 ) {
		chatiness += BOT_CHAT_CHATTY_BONUS;
	}
	if ( gameLocal.random.RandomFloat() >= chatiness ) {
		return;
	}

	botChatTokens_t tokens;

	idStr mapName = gameLocal.GetMapName();
	mapName.StripPath();
	mapName.StripFileExtension();

	tokens.self		= name;
	tokens.other	= ( other && other[0] ) ? other : "";
	tokens.weapon	= ( weapon && weapon[0] ) ? weapon : "";
	tokens.map		= mapName;
	tokens.item		= ( item && item[0] ) ? item : "";

	idStr line;

	// Silence is a valid answer: a character with nothing to say about an event
	// says nothing, and no fallback line is invented for it.  Asked before the
	// throttle, because AllowChat stamps as it answers and a line that was
	// never going to exist must not spend the bot's next six seconds of voice.
	if ( !botCharacterManager.ChatLine( character, event, tokens, line ) ) {
		return;
	}

	// The engine has no chat flood protection anywhere, and a client whose
	// reliable message queue overflows is dropped by the server - so unbounded
	// bot chatter can kick real players off.  The throttle is the manager's
	// because half of it is server wide.
	if ( !botCharacterManager.AllowChat( clientNum ) ) {
		return;
	}

	chatPending		= line;
	chatTeamOnly	= false;
	chatPendingIsReply = false;
	chatSendTime	= BotChatSendTime( line, traits.chatDelayScale );
}

/*
================
rvBot::TryQueueReply

Conversational replies share the event-chat delivery path, but never replace a
line the bot has already started "typing".  Returning success lets the manager
stamp the source player's cooldown only when a real line was reserved.
================
*/
bool rvBot::TryQueueReply( const idStr &normalizedText, bool sourceIsBot, bool addressed,
						   const char *other, bool teamOnly ) {
	if ( !active || !character || !chatPending.IsEmpty() ) {
		return false;
	}
	if ( !gameLocal.isMultiplayer || gameLocal.isClient || !gameLocal.isServer ) {
		return false;
	}
	if ( bot_chat.GetInteger() <= 0 || !bot_characters.GetBool() ) {
		return false;
	}

	float chatiness = traits.chatiness;
	if ( bot_chat.GetInteger() >= 2 ) {
		chatiness += BOT_CHAT_CHATTY_BONUS;
	}
	if ( gameLocal.random.RandomFloat() >= chatiness ) {
		return false;
	}

	botChatTokens_t tokens;
	idStr mapName = gameLocal.GetMapName();
	mapName.StripPath();
	mapName.StripFileExtension();

	tokens.self = name;
	tokens.other = ( other && other[0] ) ? other : "";
	tokens.map = mapName;

	idStr line;
	if ( !botCharacterManager.ReplyLine( character, normalizedText, sourceIsBot, addressed, tokens, line ) ) {
		return false;
	}
	if ( !botCharacterManager.AllowChat( clientNum ) ) {
		return false;
	}

	chatPending = line;
	chatTeamOnly = teamOnly;
	chatPendingIsReply = true;
	chatSendTime = BotChatSendTime( line, traits.chatDelayScale );

	return true;
}

/*
================
rvBot::UpdateChat
================
*/
void rvBot::UpdateChat( idPlayer *self ) {
	// Quake 4 already snapshots isChatting and draws mtr_icon_chatting above
	// living remote players.  Drive that stock path for the whole CPM delay so
	// clients see the same feedback they see while a person composes a line.
	if ( self ) {
		self->isChatting = active && !chatPending.IsEmpty() && gameLocal.time < chatSendTime;
	}

	if ( chatPending.IsEmpty() || gameLocal.time < chatSendTime ) {
		return;
	}

	const idStr line = chatPending;
	const bool pendingTeamOnly = chatTeamOnly;
	const bool wasReply = chatPendingIsReply;

	chatPending.Clear();
	chatTeamOnly = false;
	chatPendingIsReply = false;

	// The bot left, or its slot went away, between queuing the line and saying
	// it.  ProcessChatMessage would drop it anyway; dropping it here says why.
	if ( !self || !active ) {
		return;
	}

	// Event chatter is global, while a reply mirrors the chat it answers.  Keep
	// the game type test because team = true in a free-for-all colours every
	// speaker as a Marine and hides the line from spectators.
	const bool teamOnly = ( pendingTeamOnly && gameLocal.IsTeamGame() );

	gameLocal.mpGame.ProcessChatMessage( clientNum, teamOnly, BotDisplayName( self ), line.c_str(), NULL, !wasReply );
}

/*
================
rvBot::SayFarewell

The one line that cannot wait: the slot is about to be kicked, so a queued
farewell would be dropped with the bot that owed it.
================
*/
void rvBot::SayFarewell( void ) {
	idPlayer *self = GetPlayer();

	if ( !self ) {
		return;
	}

	// Anything still queued is about to be lost with the slot anyway, and
	// forcing it out now would have the bot answer a two second old frag on its
	// way out of the door.  The farewell replaces it rather than queue-jumping.
	chatPending.Clear();
	chatTeamOnly = false;
	chatPendingIsReply = false;

	QueueChat( BOTCHAT_FAREWELL, NULL, NULL, NULL );

	chatSendTime = gameLocal.time;
	UpdateChat( self );
}

/*
================
rvBot::OnKilledEnemy

methodOfDeath below MAX_WEAPONS is a weapon slot on the KILLER, which is this
bot, so its own spawnArgs are what name the weapon.
================
*/
void rvBot::OnKilledEnemy( idPlayer *victim, int methodOfDeath ) {
	idPlayer *self = GetPlayer();

	killStreak++;

	idStr	weaponName;
	bool	gauntlet = false;

	if ( self && methodOfDeath >= 0 && methodOfDeath < MAX_WEAPONS ) {
		const char *weaponClass = self->spawnArgs.GetString( va( "def_weapon%d", methodOfDeath ), "" );

		if ( weaponClass[0] ) {
			gauntlet	= ( idStr::Icmp( weaponClass, "weapon_gauntlet" ) == 0 );
			weaponName	= weaponClass;
			weaponName.StripLeadingOnce( "weapon_" );
		}
	}

	// One line per death, most specific first: a revenge kill is more
	// interesting than a humiliation, and both are more interesting than the
	// third frag in a row.
	rvBotChatEvent event = BOTCHAT_KILL;

	if ( victim && victim->entityNumber == revengeTarget ) {
		event			= BOTCHAT_KILL_REVENGE;
		revengeTarget	= -1;
	} else if ( gauntlet ) {
		event = BOTCHAT_KILL_GAUNTLET;
	} else if ( killStreak >= BOT_KILL_STREAK ) {
		event = BOTCHAT_KILL_STREAK;
	}

	QueueChat( event, BotDisplayName( victim ), weaponName.c_str(), NULL );
}

/*
================
rvBot::OnKilledBy

idPlayer::lastKiller would answer "who killed me" without a hook, but it is
never cleared on a normal respawn, so a bot reading it would owe the same
player a debt forever.  The debt is tracked here instead, and paid off the
moment it is collected.
================
*/
void rvBot::OnKilledBy( idPlayer *killer, int methodOfDeath ) {
	idPlayer *self = GetPlayer();

	killStreak = 0;

	if ( !killer || killer == self ) {
		// Nobody to blame: the map, a fall, or its own splash damage.
		revengeTarget = -1;
		QueueChat( BOTCHAT_DEATH_ACCIDENT, NULL, NULL, NULL );
		return;
	}

	idStr weaponName;

	if ( methodOfDeath >= 0 && methodOfDeath < MAX_WEAPONS ) {
		const char *weaponClass = killer->spawnArgs.GetString( va( "def_weapon%d", methodOfDeath ), "" );

		if ( weaponClass[0] ) {
			weaponName = weaponClass;
			weaponName.StripLeadingOnce( "weapon_" );
		}
	}

	revengeTarget = killer->entityNumber;

	QueueChat( BOTCHAT_DEATH, BotDisplayName( killer ), weaponName.c_str(), NULL );
}

/*
================
rvBot::OnDamaged

Damage is a perception event: it identifies the actual threat without granting
line-of-sight through a wall, and schedules one dodge that sustained fire may
not postpone.
================
*/
void rvBot::OnDamaged( idPlayer *attacker, int damage, const idVec3 &dir ) {
	idPlayer *self = GetPlayer();
	if ( !self || damage <= 0 ) {
		return;
	}

	damageStamp = self->lastDmgTime;
	ScheduleDodge( dir );

	if ( !IsEnemy( self, attacker ) ) {
		return;
	}

	lastAttacker = attacker;
	lastAttackerTime = gameLocal.time;

	idVec3 visiblePoint;
	if ( CanSee( self, attacker, &visiblePoint ) ) {
		const bool fresh = ( enemy.GetEntity() != attacker );
		if ( fresh ) {
			AcquireEnemy( self, attacker, true );
		}
		enemyLastSeenTime = gameLocal.time;
		enemyLastSeenOrigin = attacker->GetPhysics()->GetOrigin();
		enemyLastSeenVelocity = attacker->GetPhysics()->GetLinearVelocity();
		enemyVisiblePoint = visiblePoint;
	}
}

/*
================
rvBot::OnMatchStart
================
*/
void rvBot::OnMatchStart( void ) {
	killStreak		= 0;
	revengeTarget	= -1;

	QueueChat( BOTCHAT_LEVELSTART, NULL, NULL, NULL );
}

/*
================
rvBot::OnMatchEnd
================
*/
void rvBot::OnMatchEnd( bool won ) {
	QueueChat( won ? BOTCHAT_MATCH_WIN : BOTCHAT_MATCH_LOSE, NULL, NULL, NULL );
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

	// Talking is not an input and does not stop for bot_pause, for death or for
	// spectating: a bot that was fragged one second after its own kill should
	// still get the line out.
	UpdateChat( self );

	if ( !self || bot_pause.GetBool() ) {
		return;
	}

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

	// OnSpawn is where the personality is re-resolved, so a mid-match change to
	// bot_skill, bot_skillVariance or bot_characters lands here - on the next
	// life, not in the middle of the fight the bot is already in.
	if ( goalType == BOTGOAL_NONE ) {
		OnSpawn();
		goalType = BOTGOAL_ROAM;
	}

	UpdateEnemy( self );
	UpdateGoal( self );
	UpdateAim( self, cmd );
	// Movement is resolved against the view angles in this same command.  Aim
	// first so a sharp turn cannot rotate the world-space route input away from
	// a narrow takeoff or landing during the frame it matters most.
	UpdateMovement( self, cmd );
	UpdateWeapon( self );
	UpdateFire( self, cmd );

	if ( bot_debug.GetInteger() >= 2 && gameLocal.time >= nextStatusTime ) {
		static const char *goalNames[] = { "none", "roam", "item", "enemy", "objective" };
		static const char *objectiveNames[] = { "none", "capture", "intercept", "return", "escort", "fetch", "rescue", "control", "defend" };
		static const char *mistakeNames[] = { "-", "losetrack", "mistimed", "wrongweapon", "latedodge" };

		nextStatusTime = gameLocal.time + 2000;

		idPlayer *foe = enemy.GetEntity();
		idStr goalLabel = goalNames[idMath::ClampInt( 0, 4, goalType )];
		if ( goalType == BOTGOAL_OBJECTIVE ) {
			goalLabel += ":";
			goalLabel += objectiveNames[idMath::ClampInt( 0, 8, objectiveKind )];
		}

		gameLocal.Printf( "bot %s [%s/%s skill %.1f]: at %s hp %d armor %d goal %s corner %d/%d remain %.0f enemy %s mistake %s%s\n",
						  name.c_str(),
						  character ? character->GetName() : "-",
						  character ? character->GetStyleName() : "-",
						  effectiveSkill,
						  self->GetPhysics()->GetOrigin().ToString( 0 ),
						  self->health,
						  self->inventory.armor,
						  goalLabel.c_str(),
						  pathCorner, path.Num(),
						  PathDistanceRemaining( self->GetPhysics()->GetOrigin() ),
						  foe ? BotDisplayName( foe ) : "-",
						  mistakeNames[idMath::ClampInt( 0, BOTMISTAKE_NUM - 1, ( gameLocal.time < mistakeEndTime ) ? mistake : BOTMISTAKE_NONE )],
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
	pendingSkill		= -1;
	pendingExactCharacter = false;
	leaderClientNum		= -1;
	nextLeaderCheck		= 0;
	ResetReplyCooldowns();
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

	// Every bot released its own character above; this is the belt and braces
	// for a roster that was reloaded out from under them.
	botCharacterManager.ReleaseAllCharacters();

	navBuilt			= false;
	navFailed			= false;
	nextMinPlayerCheck	= 0;
	pendingSkill		= -1;
	pendingExactCharacter = false;
	leaderClientNum		= -1;
	nextLeaderCheck		= 0;
	ResetReplyCooldowns();
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

	// Bots keep their client slots and their characters across a map change;
	// only the map-shaped state goes.
	leaderClientNum	= -1;
	nextLeaderCheck	= 0;
	ResetReplyCooldowns();

	// Surviving bots re-arm.  Empty slots are skipped now that OnSpawn resolves
	// a personality: an inactive slot has clientNum -1, which is not a seed.
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( bots[i].IsActive() ) {
			bots[i].OnSpawn();
		}
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
rvBotManager::ResetReplyCooldowns
================
*/
void rvBotManager::ResetReplyCooldowns( void ) {
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		nextReplySourceTime[i] = 0;
	}
}

/*
================
rvBotManager::OnChatMessage

One accepted typed-chat line gets at most one prospective responder.  Addressed
bots are preferred, team chat is visible only to team-mates, and the packet's
claimed name is never used: clientNum and server-owned userinfo are the
identity.  The selected character can still decline through source/address
rules, chatiness, a pending line or the shared flood throttle.
================
*/
void rvBotManager::OnChatMessage( int sourceClientNum, bool teamOnly, const char *visibleText ) {
	if ( sourceClientNum < 0 || sourceClientNum >= MAX_CLIENTS ||
		 bot_chat.GetInteger() <= 0 || !bot_characters.GetBool() ) {
		return;
	}
	if ( gameLocal.time < nextReplySourceTime[sourceClientNum] ) {
		return;
	}

	idEntity *sourceEntity = gameLocal.entities[sourceClientNum];
	if ( !sourceEntity || !sourceEntity->IsType( idPlayer::GetClassType() ) ) {
		return;
	}

	idPlayer *sourcePlayer = static_cast<idPlayer *>( sourceEntity );
	idStr normalizedText;
	botCharacterManager.NormalizeReplyText( visibleText, normalizedText );

	if ( normalizedText.IsEmpty() ) {
		return;
	}

	const bool sourceIsBot = IsBot( sourceClientNum );
	idList<int> addressedCandidates;
	idList<int> generalCandidates;

	for ( int i = 0; i < gameLocal.numClients; i++ ) {
		if ( i == sourceClientNum || !bots[i].IsActive() ) {
			continue;
		}

		idEntity *candidateEntity = gameLocal.entities[i];
		if ( !candidateEntity || !candidateEntity->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}

		idPlayer *candidatePlayer = static_cast<idPlayer *>( candidateEntity );
		if ( candidatePlayer->spectating ) {
			continue;
		}
		if ( teamOnly &&
			 ( !gameLocal.IsTeamGame() || candidatePlayer->team != sourcePlayer->team ) ) {
			continue;
		}
		if ( candidatePlayer->IsPlayerMuted( sourcePlayer ) ) {
			continue;
		}

		const rvBotCharacter *candidateCharacter = bots[i].GetCharacter();
		const char *candidateName = gameLocal.userInfo[i].GetString( "ui_name", bots[i].GetName() );
		const bool addressed = botCharacterManager.ReplyNameMatches( candidateName, normalizedText );

		if ( !botCharacterManager.HasReply( candidateCharacter, normalizedText, sourceIsBot, addressed ) ) {
			continue;
		}

		if ( addressed ) {
			addressedCandidates.Append( i );
		} else {
			generalCandidates.Append( i );
		}
	}

	const idList<int> *candidates = addressedCandidates.Num() ? &addressedCandidates : &generalCandidates;
	if ( !candidates->Num() ) {
		return;
	}

	const bool addressed = addressedCandidates.Num() > 0;
	float replyChance;

	if ( sourceIsBot ) {
		replyChance = addressed ? BOT_REPLY_BOT_ADDRESSED_CHANCE : BOT_REPLY_BOT_CHANCE;
	} else {
		replyChance = addressed ? BOT_REPLY_PLAYER_ADDRESSED_CHANCE : BOT_REPLY_PLAYER_CHANCE;
	}

	if ( gameLocal.random.RandomFloat() >= replyChance ) {
		return;
	}

	const int responder = ( *candidates )[gameLocal.random.RandomInt( candidates->Num() )];
	const char *sourceName = gameLocal.userInfo[sourceClientNum].GetString( "ui_name", "player" );

	if ( !bots[responder].TryQueueReply( normalizedText, sourceIsBot, addressed, sourceName, teamOnly ) ) {
		return;
	}

	int cooldown = BOT_REPLY_SOURCE_THROTTLE_MSEC;
	if ( bot_chat.GetInteger() >= 2 ) {
		cooldown /= 2;
	}
	nextReplySourceTime[sourceClientNum] = gameLocal.time + cooldown;
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
rvBotManager::GoalClaimCount

Lightweight team reservation.  Callers turn this count into a utility penalty,
so a second bot may still help with a major objective but ordinary pickups are
naturally spread across the team.
================
*/
int rvBotManager::GoalClaimCount( const rvBot *requester, const idEntity *goal ) const {
	if ( !requester || !goal ) {
		return 0;
	}

	const int requesterNum = requester->GetClientNum();
	idPlayer *requesterPlayer = ( requesterNum >= 0 && requesterNum < gameLocal.numClients &&
		gameLocal.entities[requesterNum] && gameLocal.entities[requesterNum]->IsType( idPlayer::GetClassType() ) ) ?
		static_cast<idPlayer *>( gameLocal.entities[requesterNum] ) : NULL;
	if ( !requesterPlayer ) {
		return 0;
	}
	if ( !gameLocal.IsTeamGame() ) {
		return 0;
	}

	int claims = 0;
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( &bots[i] == requester || !bots[i].IsActive() || bots[i].GetGoalEntity() != goal ) {
			continue;
		}

		idEntity *ent = gameLocal.entities[i];
		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}
		idPlayer *player = static_cast<idPlayer *>( ent );
		if ( player->GetInstance() != requesterPlayer->GetInstance() ) {
			continue;
		}
		if ( player->team != requesterPlayer->team ) {
			continue;
		}

		claims++;
	}

	return claims;
}

/*
================
rvBotManager::BalancedTeamForBot

Bot allocation is synchronous enough for the rvBot to exist, but not for
gameLocal.numClients or even the idPlayer entity to be current.  Count the full
client-slot range and include the explicit reservations made by earlier bot
allocations.  Without that second source, several addbot commands buffered in
one frame all see the same population and choose the same side.
================
*/
int rvBotManager::BalancedTeamForBot( int clientNum, int &balanceInstance ) const {
	balanceInstance = 0;

	if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
		idPlayer *botPlayer = bots[clientNum].GetPlayer();

		if ( botPlayer ) {
			balanceInstance = botPlayer->GetInstance();
		} else {
			idPlayer *localPlayer = gameLocal.GetLocalPlayer();

			// A console-created bot on a listen server joins the host's gameplay
			// instance.  Dedicated/team maps use instance zero.
			if ( localPlayer ) {
				balanceInstance = localPlayer->GetInstance();
			}
		}
	}

	int teamCount[TEAM_MAX];
	memset( teamCount, 0, sizeof( teamCount ) );

	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( i == clientNum ) {
			continue;
		}

		idEntity *ent = gameLocal.entities[i];
		idPlayer *player = ( ent && ent->IsType( idPlayer::GetClassType() ) ) ?
			static_cast<idPlayer *>( ent ) : NULL;

		if ( player ) {
			if ( player->GetInstance() != balanceInstance ) {
				continue;
			}
			if ( player->spectating && player->wantSpectate ) {
				continue;
			}

			int assignedTeam = player->team;

			// Active bots and players waiting for their first/round spawn have
			// an authoritative intended team before idPlayer::team is latched.
			// Prefer that reservation during this narrow window.
			if ( bots[i].active || player->spectating ) {
				const char *intendedName = gameLocal.userInfo[i].GetString( "ui_team" );

				for ( int team = 0; team < TEAM_MAX; team++ ) {
					if ( !idStr::Icmp( intendedName, idMultiplayerGame::teamNames[team] ) ) {
						assignedTeam = team;
						break;
					}
				}
			}

			if ( assignedTeam >= 0 && assignedTeam < TEAM_MAX ) {
				teamCount[assignedTeam]++;
			}
			continue;
		}

		// AllocateClientSlotForBot can register rvBot before SpawnPlayer has
		// produced its idPlayer.  Its cached assignment is still a real place
		// on this instance and must influence the next buffered addbot.
		if ( bots[i].active && bots[i].teamAssignmentInstance == balanceInstance &&
			 bots[i].teamAssignment >= 0 && bots[i].teamAssignment < TEAM_MAX ) {
			teamCount[bots[i].teamAssignment]++;
		}
	}

	return ( teamCount[TEAM_STROGG] < teamCount[TEAM_MARINE] ) ? TEAM_STROGG : TEAM_MARINE;
}

/*
================
rvBotManager::FillUserInfo
================
*/
void rvBotManager::FillUserInfo( int clientNum, idDict &info ) {
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

Only used when there is no roster to pick from: with characters loaded the name
comes from the character, so the scoreboard and the personality agree.
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
bool rvBotManager::AddBot( const char *name, int skillLevel, bool requireExactCharacter ) {
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

	// -1 is "follow bot_skill".  Anything else has to be a real level, or an
	// operator's typo would quietly become one.
	if ( skillLevel != -1 && ( skillLevel < 1 || skillLevel > BOT_SKILL_LEVELS ) ) {
		gameLocal.Printf( "addbot: skill must be 1 to %d\n", BOT_SKILL_LEVELS );
		return false;
	}
	if ( requireExactCharacter ) {
		if ( !name || !name[0] ) {
			gameLocal.Printf( "addbot: exact character mode requires a name\n" );
			return false;
		}
		if ( !bot_characters.GetBool() ) {
			gameLocal.Printf( "addbot: exact character '%s' requires bot_characters 1\n", name );
			return false;
		}

		const rvBotCharacter *requestedCharacter = botCharacterManager.FindCharacter( name );
		if ( !requestedCharacter ) {
			gameLocal.Printf( "addbot: no character called '%s' is loaded\n", name );
			return false;
		}
		if ( requestedCharacter->IsInUse() ) {
			gameLocal.Printf( "addbot: character '%s' is already in use\n", name );
			return false;
		}
	}

	if ( !EnsureNavMesh() ) {
		gameLocal.Printf( "addbot: no navigation could be generated for this map\n" );
		return false;
	}

	const int level = idMath::ClampInt( 1, BOT_SKILL_LEVELS, ( skillLevel > 0 ) ? skillLevel : bot_skill.GetInteger() );

	// The engine wants a name for the slot before the bot exists.  Peek at the
	// roster for one rather than inventing it, so the slot, the scoreboard and
	// the personality all agree.  The peek and the claim that follows it can
	// only disagree if the roster changed in between, and FillUserInfo
	// republishes ui_name from the character anyway, so the worst case is that
	// the engine's own record of the slot name is cosmetically stale.
	const char *botName = ( name && name[0] ) ? name : NULL;

	if ( !botName && bot_characters.GetBool() ) {
		const rvBotCharacter *peek = botCharacterManager.PickCharacter( level, false );

		if ( peek ) {
			botName = peek->GetName();
		}
	}
	if ( !botName ) {
		botName = PickBotName();
	}

	int maxPlayers = gameLocal.serverInfo.GetInt( "si_maxPlayers" );
	if ( maxPlayers <= 0 ) {
		maxPlayers = MAX_CLIENTS;
	}

	// The engine allocates the slot, then calls back into ServerClientBegin ->
	// SpawnPlayer, which is where OnSpawnPlayer registers the bot.  A per-bot
	// skill has nowhere to travel through that call, so it is parked here for
	// the length of it.
	pendingSkill = skillLevel;
	pendingExactCharacter = requireExactCharacter;

	const int clientNum = networkSystem->AllocateClientSlotForBot( botName, maxPlayers );

	pendingSkill = -1;
	pendingExactCharacter = false;

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		gameLocal.Printf( "addbot: no free client slot\n" );
		return false;
	}

	if ( !bots[clientNum].IsActive() ) {
		gameLocal.Warning( "addbot: client %d was allocated but never spawned a bot", clientNum );
		return false;
	}

	gameLocal.Printf( "addbot: '%s' joined as client %d at skill %.1f\n",
					  bots[clientNum].GetName(), clientNum, bots[clientNum].GetEffectiveSkill() );

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

	// Said before the kick, and said immediately: a queued line would be
	// dropped along with the slot that owes it.
	bots[target].SayFarewell();

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
rvBotManager::RebindCharacters
================
*/
void rvBotManager::RebindCharacters( void ) {
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( bots[i].IsActive() ) {
			bots[i].RebindCharacter();
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

	gameLocal.Printf( "  cl  name             character        style        skill\n" );

	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( !bots[i].IsActive() ) {
			continue;
		}

		const rvBotCharacter *character = bots[i].GetCharacter();

		gameLocal.Printf( "  %2d  %-16s %-16s %-12s %.1f\n",
						  i,
						  bots[i].GetName(),
						  character ? character->GetName() : "-",
						  character ? character->GetStyleName() : "-",
						  bots[i].GetEffectiveSkill() );
		count++;
	}

	gameLocal.Printf( "%d bot%s, %d character%s loaded, navmesh %s (%d nodes, %d links, built in %d ms)\n",
					  count, count == 1 ? "" : "s",
					  botCharacterManager.NumCharacters(), botCharacterManager.NumCharacters() == 1 ? "" : "s",
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
		bots[clientNum].Init( clientNum, botName, pendingSkill, pendingExactCharacter );
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

	if ( leaderClientNum == clientNum ) {
		leaderClientNum = -1;
	}
	nextReplySourceTime[clientNum] = 0;

	bots[clientNum].Shutdown();
}

/*
================
rvBotManager::OnMatchStart
================
*/
void rvBotManager::OnMatchStart( void ) {
	if ( !gameLocal.isMultiplayer || gameLocal.isClient ) {
		return;
	}

	// Scores are zeroed on the way into GAMEON, so nobody leads yet.
	leaderClientNum	= -1;
	nextLeaderCheck	= 0;

	botCharacterManager.ResetChatThrottle();
	ResetReplyCooldowns();

	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( bots[i].IsActive() ) {
			bots[i].OnMatchStart();
		}
	}
}

/*
================
rvBotManager::OnMatchEnd
================
*/
void rvBotManager::OnMatchEnd( void ) {
	if ( !gameLocal.isMultiplayer || gameLocal.isClient ) {
		return;
	}

	// Red Rover uses teams for combat rules but ranks the match individually;
	// using teamScore there makes every bot on the final team claim a win.
	const bool useTeamScore = gameLocal.IsTeamGame() && gameLocal.gameType != GAME_REDROVER;
	const int forfeitWinner = useTeamScore ? gameLocal.mpGame.ForfeitTeam() : -1;
	bool topScoreFound = false;
	int topScore = 0;
	int topScoreCount = 0;

	if ( useTeamScore ) {
		for ( int i = 0; i < TEAM_MAX; i++ ) {
			const int score = gameLocal.mpGame.GetScoreForTeam( i );

			if ( !topScoreFound || score > topScore ) {
				topScore = score;
				topScoreFound = true;
				topScoreCount = 1;
			} else if ( score == topScore ) {
				topScoreCount++;
			}
		}
	} else {
		for ( int i = 0; i < gameLocal.numClients; i++ ) {
			idEntity *ent = gameLocal.entities[i];

			if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
				continue;
			}

			idPlayer *player = static_cast<idPlayer *>( ent );
			if ( !gameLocal.mpGame.CanPlay( player ) ||
				 ( gameLocal.gameType == GAME_DUEL && player->spectating ) ) {
				continue;
			}

			const int score = gameLocal.mpGame.GetScore( i );

			if ( !topScoreFound || score > topScore ) {
				topScore = score;
				topScoreFound = true;
				topScoreCount = 1;
			} else if ( score == topScore ) {
				topScoreCount++;
			}
		}
	}

	// Equal points are not a draw when one side has forfeited.  In that case the
	// surviving team is the authoritative winner even on a 0-0 board.
	const bool matchTied = topScoreFound && topScoreCount > 1 && forfeitWinner < 0;

	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( !bots[i].IsActive() ) {
			continue;
		}
		if ( matchTied ) {
			// There is no stock draw-chat category.  Silence is preferable to
			// every tied bot claiming a win or half of them conceding a loss.
			continue;
		}

		idEntity *ent = gameLocal.entities[i];
		idPlayer *player = ( ent && ent->IsType( idPlayer::GetClassType() ) ) ? static_cast<idPlayer *>( ent ) : NULL;
		if ( !player || !gameLocal.mpGame.CanPlay( player ) ||
			 ( gameLocal.gameType == GAME_DUEL && player->spectating ) ) {
			// Duel's waiting line did not contest this match and must not announce
			// a loss merely because it watched from spectator.
			continue;
		}

		bool won = false;

		// Zero and negative scoreboards are valid too, so no positive-score
		// sentinel is used.
		const bool validTeam = player->team >= 0 && player->team < TEAM_MAX;
		if ( useTeamScore && forfeitWinner >= 0 ) {
			won = validTeam && player->team == forfeitWinner;
		} else {
			const int score = ( useTeamScore && validTeam ) ?
				gameLocal.mpGame.GetScoreForTeam( player->team ) :
				gameLocal.mpGame.GetScore( i );

			won = topScoreFound && ( !useTeamScore || validTeam ) && score >= topScore;
		}

		bots[i].OnMatchEnd( won );
	}

	leaderClientNum = -1;
}

/*
================
rvBotManager::OnPlayerDeath

Hooked from idMultiplayerGame::PlayerDeath, after the scores have been
committed, so a bot reacting here already sees the post-frag board.
================
*/
void rvBotManager::OnPlayerDeath( idPlayer *dead, idPlayer *killer, int methodOfDeath ) {
	if ( !gameLocal.isMultiplayer || gameLocal.isClient || !dead ) {
		return;
	}

	const int deadNum	= dead->entityNumber;
	const int killerNum	= killer ? killer->entityNumber : -1;

	if ( IsBot( deadNum ) ) {
		bots[deadNum].OnKilledBy( killer, methodOfDeath );
	}

	// A suicide is a death and nothing else: nobody gets a kill line for it.
	if ( killer && killer != dead && IsBot( killerNum ) ) {
		bots[killerNum].OnKilledEnemy( dead, methodOfDeath );
	}
}

/*
================
rvBotManager::OnPlayerDamaged
================
*/
void rvBotManager::OnPlayerDamaged( idPlayer *victim, idEntity *attacker, int damage, const idVec3 &dir ) {
	if ( !gameLocal.isMultiplayer || gameLocal.isClient || !victim || !IsBot( victim->entityNumber ) ) {
		return;
	}

	idPlayer *source = NULL;
	if ( attacker && attacker->IsType( idPlayer::GetClassType() ) ) {
		source = static_cast<idPlayer *>( attacker );
	} else if ( attacker && attacker->IsType( idProjectile::GetClassType() ) ) {
		idEntity *owner = static_cast<idProjectile *>( attacker )->GetOwner();
		if ( owner && owner->IsType( idPlayer::GetClassType() ) ) {
			source = static_cast<idPlayer *>( owner );
		}
	}

	bots[victim->entityNumber].OnDamaged( source, damage, dir );
}

/*
================
rvBotManager::UpdateLeaderChat

idMultiplayerGame::UpdateLeader is declared and never defined, and the lead
announcements in CommonRun are written around gameLocal.GetLocalPlayer(), so
they never fire on a dedicated server and never fire for anyone but the local
client on a listen server.  The rank the bots react to is tracked here instead.
================
*/
void rvBotManager::UpdateLeaderChat( void ) {
	if ( gameLocal.time < nextLeaderCheck ) {
		return;
	}
	nextLeaderCheck = gameLocal.time + BOT_LEADER_CHECK_MSEC;

	if ( bot_chat.GetInteger() <= 0 ) {
		return;
	}

	int leader	= -1;
	int best	= 0;
	int shared	= 0;

	for ( int i = 0; i < gameLocal.numClients; i++ ) {
		idEntity *ent = gameLocal.entities[i];

		if ( !ent || !ent->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}
		if ( static_cast<idPlayer *>( ent )->spectating ) {
			continue;
		}

		const int score = gameLocal.mpGame.GetScore( i );

		if ( leader == -1 || score > best ) {
			best	= score;
			leader	= i;
			shared	= 1;
		} else if ( score == best ) {
			shared++;
		}
	}

	// Nobody leads on nothing, and a tie is not a lead change - without both
	// tests two bots trade "first place" lines every time either of them scores.
	if ( best <= 0 || shared > 1 ) {
		leader = -1;
	}

	if ( leader == leaderClientNum ) {
		return;
	}

	const int previous = leaderClientNum;

	leaderClientNum = leader;

	if ( previous >= 0 && IsBot( previous ) ) {
		idEntity *ent = ( leader >= 0 ) ? gameLocal.entities[leader] : NULL;
		idPlayer *player = ( ent && ent->IsType( idPlayer::GetClassType() ) ) ? static_cast<idPlayer *>( ent ) : NULL;

		bots[previous].QueueChat( BOTCHAT_LEAD_LOST, BotDisplayName( player ), NULL, NULL );
	}

	if ( leader >= 0 && IsBot( leader ) ) {
		idEntity *ent = ( previous >= 0 ) ? gameLocal.entities[previous] : NULL;
		idPlayer *player = ( ent && ent->IsType( idPlayer::GetClassType() ) ) ? static_cast<idPlayer *>( ent ) : NULL;

		bots[leader].QueueChat( BOTCHAT_LEAD_TAKEN, BotDisplayName( player ), NULL, NULL );
	}
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
	UpdateLeaderChat();

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
