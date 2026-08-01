//----------------------------------------------------------------
// HitMarker.h
//
// openQ4 crosshair hit marker: four angled marks that bloom out of the
// crosshair and fade when a hit lands, in the manner of Quake Champions.
//----------------------------------------------------------------

#ifndef __GAME_HITMARKER_H__
#define __GAME_HITMARKER_H__

// What the marker knows about one hit.  The relationship to the victim and the
// killing blow come from the hit-info pipeline; the armour bit is the only fact
// the snapshot fallback carries.
typedef enum {
	HITMARKER_TEAM	= BIT( 0 ),		// victim shares the attacker's team
	HITMARKER_SELF	= BIT( 1 ),		// attacker damaged themselves
	HITMARKER_ARMOR	= BIT( 2 ),		// armour absorbed part of the hit
	HITMARKER_KILL	= BIT( 3 )		// the hit finished the victim
} hitMarkerFlags_t;

// A landed hit reaches the marker from more than one direction and the paths
// overlap, so each trigger says how much it actually knows.
typedef enum {
	HITMARKER_PRECISE = 0,	// hit-info pipeline: flags are known, amount may be
	HITMARKER_COARSE		// snapshot hit pulse: no amount, no relationship
} hitMarkerSource_t;

// The server is allowed to withhold the amount ( g_hitFeedback 1 ), and the
// snapshot path never carries one, so an unknown amount is a first class case
// rather than a zero.
const int HITMARKER_DAMAGE_UNKNOWN = -1;

/*
===============================================================================

	rvHitMarker

	One screen space pulse centred on the crosshair.  There is only ever one
	marker on screen - a second hit re-arms the pulse instead of stacking - so
	the state is a single record owned by this translation unit and reached
	through static calls, the same shape the effect has on screen.

	Everything the marker draws is a pure function of that record and the
	elapsed time, so there is no update pass to keep in step with the draw.

===============================================================================
*/
class rvHitMarker {
public:
	// map load, respawn, and anything else that must not leave a stale pulse
	static void				Clear( void );

	// staging.  damage may be HITMARKER_DAMAGE_UNKNOWN
	static void				Trigger( int damage, int flags, hitMarkerSource_t source );

	// 2D pass, drawn over the crosshair under the crosshair's own gating
	static void				Draw( void );

	// Stock Quake 4 recolours the crosshair red for 100ms on a hit.  The marker
	// replaces that cue, so the flash only runs for players who have no marker
	// or who have explicitly asked for both.
	static bool				CrosshairFlashEnabled( void );
};

#endif	/* !__GAME_HITMARKER_H__ */
