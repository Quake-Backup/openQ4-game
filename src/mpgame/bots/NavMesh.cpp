//----------------------------------------------------------------
// NavMesh.cpp
//
// Runtime navigation mesh generation and routing.  See NavMesh.h for why this
// exists instead of AAS.
//----------------------------------------------------------------

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "NavMesh.h"

rvNavMesh					navMesh;

/*
===============================================================================

	Generation constants.

	Everything that describes the agent is derived from the movement cvars the
	server is actually running, so a mod that widens the player also widens the
	navmesh without touching this file.

===============================================================================
*/

// Only CONTENTS_SOLID and CONTENTS_PLAYERCLIP: MASK_PLAYERSOLID also carries
// CONTENTS_BODY, and generation must not see players standing in doorways.
#define NAV_CLIP_MASK			( CONTENTS_SOLID | CONTENTS_PLAYERCLIP )

static const float	NAV_WALKABLE_NORMAL_Z	= 0.7f;		// ~45 degrees, matches idPhysics_Player
static const float	NAV_MAX_DROP			= 192.0f;	// furthest one-way ledge drop we will route over
static const float	NAV_MERGE_HEIGHT		= 28.0f;	// two floors this close in a cell are one node
static const float	NAV_GROUND_OFFSET		= 1.0f;		// lift traces off the surface they start on
static const int	NAV_MAX_NODES			= 65536;
static const int	NAV_SMOOTH_LOOKAHEAD	= 8;		// nodes a single string-pull step may skip
static const float	NAV_SMOOTH_MAX_DIP		= 48.0f;	// how far a shortcut may float above the ground it crosses
static const int	NAV_MAX_ROUTE_REPAIRS	= 16;		// blocked graph edges excluded before giving up

// Extra cost so a route prefers walking around to falling or jumping, when
// more than one works.
static const float	NAV_DROP_PENALTY		= 96.0f;
static const float	NAV_JUMP_PENALTY		= 64.0f;
// Volume links are near instant to traverse but should not be spammed.
static const float	NAV_JUMPPAD_COST		= 128.0f;
static const float	NAV_TELEPORT_COST		= 64.0f;

static const int	navNeighbourX[8] = { 1,  1,  0, -1, -1, -1,  0,  1 };
static const int	navNeighbourY[8] = { 0,  1,  1,  1,  0, -1, -1, -1 };

// Loaded once per build and reused for every probe; rebuilding a trace model
// per trace dominates generation time otherwise.
static idClipModel *		navAgentModel = NULL;
static idBounds				navAgentBounds;

/*
================
NavTravelVolumeBounds

Jump pads move their brush clip out of the entity physics object and into the
force-field evaluator during Spawn.  Teleporter triggers retain ordinary
physics bounds.  This keeps both generation and execution aimed at the volume
that actually activates.
================
*/
static bool NavTravelVolumeBounds( idEntity *ent, idBounds &bounds ) {
	if ( !ent ) {
		return false;
	}

	if ( ent->IsType( rvJumpPad::GetClassType() ) ) {
		return static_cast<rvJumpPad *>( ent )->GetForceFieldBounds( bounds );
	}

	if ( !ent->GetPhysics() ) {
		return false;
	}

	bounds = ent->GetPhysics()->GetAbsBounds();
	return true;
}

/*
================
NavDoorIsWalkActivated

Only an ordinary unlocked proximity door is equivalent to open floor.  Script,
key, shoot, and locked doors need decisions the movement layer cannot make and
must remain solid until their game logic opens them.
================
*/
static bool NavDoorIsWalkActivated( idEntity *ent ) {
	if ( !ent || !ent->IsType( idDoor::GetClassType() ) ) {
		return false;
	}

	idDoor *door = static_cast<idDoor *>( ent );
	return !door->IsLocked() && !door->IsNoTouch() &&
		   !door->spawnArgs.GetString( "requires", "" )[0];
}

// openQ4 BEGIN
/*
================
NavPointInLethalLiquid

Lava and slime are not solid, so every generation trace passes straight through them and the graph
would happily lay a walkable node on the bottom of a lava pool - then route bots across it. Refusing
those cells keeps routing out of the hazard entirely, which is what Quake 3's AAS achieves by
classifying liquid areas up front.

Water is deliberately not excluded. It is crossable, so it stays in the graph and swimming is
handled at run time by rvBot::UpdateLiquidMovement.
================
*/
static bool NavPointInLethalLiquid( const idVec3 &point ) {
	const idVec3 probe = point + idVec3( 0.0f, 0.0f, NAV_GROUND_OFFSET );
	return ( gameLocal.LiquidContentsAtPoint( probe, NULL ) & ( CONTENTS_LAVA | CONTENTS_SLIME ) ) != 0;
}
// openQ4 END

/*
================
NavTranslationThroughDoors

A navmesh built while a normal multiplayer door is closed must still contain
the floor on its far side.  Players activate these doors simply by approaching
them, so they are traversable rather than permanent walls.  Repeat a blocked
trace while ignoring at most two door leaves; all other solid entities remain
real obstructions and are reconsidered by route searches at run time.
================
*/
static bool NavTranslationThroughDoors( trace_t &trace, const idVec3 &start,
										const idVec3 &end, const idClipModel *model ) {
	const idEntity *passEntity = NULL;
	const idEntity *passEntity2 = NULL;

	for ( int attempt = 0; attempt < 3; attempt++ ) {
		gameLocal.Translation( NULL, trace, start, end, model, mat3_identity,
						   NAV_CLIP_MASK, passEntity, passEntity2 );
		if ( trace.fraction >= 1.0f ) {
			return true;
		}

		if ( trace.c.entityNum < 0 || trace.c.entityNum >= MAX_GENTITIES ) {
			return false;
		}

		idEntity *blocker = gameLocal.entities[trace.c.entityNum];
		if ( !NavDoorIsWalkActivated( blocker ) ||
			 blocker == passEntity || blocker == passEntity2 ) {
			return false;
		}

		if ( !passEntity ) {
			passEntity = blocker;
		} else if ( !passEntity2 ) {
			passEntity2 = blocker;
		} else {
			return false;
		}
	}

	return false;
}

/*
================
NavEntityTranslationThroughDoors

The sampled graph remains authoritative for static world geometry.  Runtime
route repair only needs to discover solid entities that have moved into a link
since generation; re-sampling the floor and stair geometry makes a stricter,
different test than the one which created the edge.  Normal touch doors are
ignored because approaching them is the action that opens them.
================
*/
static bool NavEntityTranslationThroughDoors( trace_t &trace, const idVec3 &start,
										  const idVec3 &end, const idClipModel *model ) {
	const idEntity *passEntity = NULL;
	const idEntity *passEntity2 = NULL;

	for ( int attempt = 0; attempt < 3; attempt++ ) {
		gameLocal.TranslationEntities( NULL, trace, start, end, model, mat3_identity,
									  NAV_CLIP_MASK, passEntity, passEntity2 );
		if ( trace.fraction >= 1.0f ) {
			return true;
		}

		if ( trace.c.entityNum < 0 || trace.c.entityNum >= MAX_GENTITIES ) {
			return false;
		}

		idEntity *blocker = gameLocal.entities[trace.c.entityNum];
		if ( !NavDoorIsWalkActivated( blocker ) ||
			 blocker == passEntity || blocker == passEntity2 ) {
			return false;
		}

		if ( !passEntity ) {
			passEntity = blocker;
		} else if ( !passEntity2 ) {
			passEntity2 = blocker;
		} else {
			return false;
		}
	}

	return false;
}

/*
================
rvNavMesh::GetAgentBounds

The player's own standing bounding box, origin at the feet.
================
*/
const idBounds &rvNavMesh::GetAgentBounds( void ) {
	const float halfWidth = pm_bboxwidth.GetFloat() * 0.5f;

	navAgentBounds[0].Set( -halfWidth, -halfWidth, 0.0f );
	navAgentBounds[1].Set( halfWidth, halfWidth, pm_normalheight.GetFloat() );

	return navAgentBounds;
}

/*
===============================================================================

	rvNavPath

===============================================================================
*/

/*
================
rvNavPath::Append
================
*/
void rvNavPath::Append( const idVec3 &origin, int travelType, int actionEntityNum ) {
	navCorner_t corner;

	corner.origin			= origin;
	corner.travelType		= (short)travelType;
	corner.actionEntityNum	= (short)actionEntityNum;

	corners.Append( corner );
}

/*
================
rvNavPath::Length
================
*/
float rvNavPath::Length( void ) const {
	float length = 0.0f;

	for ( int i = 1; i < corners.Num(); i++ ) {
		length += ( corners[i].origin - corners[i - 1].origin ).Length();
	}

	return length;
}

/*
================
rvNavPath::LengthFrom

Length() describes only the geometry between stored corners.  A route created
by FindPath also retains its graph cost, including the exact first/last legs,
jump/drop penalties, and cheap authored transports.  The geometric fallback
keeps manually assembled paths useful from another origin.
================
*/
float rvNavPath::LengthFrom( const idVec3 &origin ) const {
	if ( corners.Num() == 0 ) {
		return 0.0f;
	}
	if ( hasTravelCost && ( origin - travelStart ).LengthSqr() <= 1.0f ) {
		return travelCost;
	}

	return ( corners[0].origin - origin ).Length() + Length();
}

/*
================
rvNavPath::SetTravelCost

String pulling removes intermediate nodes, but their jump/drop penalties and
cheap transport edges still matter when comparing goals.  Keep the exact graph
cost alongside the steering corners without changing the movement interface.
================
*/
void rvNavPath::SetTravelCost( const idVec3 &origin, float cost ) {
	travelStart = origin;
	travelCost = Max( 0.0f, cost );
	hasTravelCost = true;
}

/*
===============================================================================

	rvNavMesh construction

===============================================================================
*/

/*
================
rvNavMesh::rvNavMesh
================
*/
rvNavMesh::rvNavMesh( void ) {
	gridOrigin.Zero();
	cellSize		= 24.0f;
	numAreas		= 0;
	buildMsec		= 0;
	heuristicScale = 1.0f;
	searchSerial	= 0;

	nodes.SetGranularity( 1024 );
	links.SetGranularity( 4096 );
}

/*
================
rvNavMesh::~rvNavMesh
================
*/
rvNavMesh::~rvNavMesh( void ) {
	Clear();
}

/*
================
rvNavMesh::Clear
================
*/
void rvNavMesh::Clear( void ) {
	nodes.Clear();
	links.Clear();
	nodeHash.Clear();

	gScore.Clear();
	fScore.Clear();
	cameFrom.Clear();
	openTag.Clear();
	closedTag.Clear();
	heap.Clear();

	numAreas		= 0;
	buildMsec		= 0;
	heuristicScale = 1.0f;
	searchSerial	= 0;

	if ( navAgentModel ) {
		delete navAgentModel;
		navAgentModel = NULL;
	}
}

/*
================
rvNavMesh::CellCoords
================
*/
void rvNavMesh::CellCoords( const idVec3 &origin, int &gx, int &gy ) const {
	gx = (int)idMath::Floor( ( origin.x - gridOrigin.x ) / cellSize );
	gy = (int)idMath::Floor( ( origin.y - gridOrigin.y ) / cellSize );
}

/*
================
rvNavMesh::CellCentre
================
*/
idVec3 rvNavMesh::CellCentre( int gx, int gy, float z ) const {
	return idVec3( gridOrigin.x + ( gx + 0.5f ) * cellSize,
				   gridOrigin.y + ( gy + 0.5f ) * cellSize,
				   z );
}

/*
================
rvNavMesh::FindNode

Nodes are hashed by grid cell.  A cell can hold several floors - a catwalk over
a corridor - so the z has to be matched as well.
================
*/
int rvNavMesh::FindNode( int gx, int gy, float z ) const {
	const int key = nodeHash.GenerateKey( gx, gy );

	for ( int i = nodeHash.First( key ); i != -1; i = nodeHash.Next( i ) ) {
		if ( nodes[i].gx != gx || nodes[i].gy != gy ) {
			continue;
		}
		if ( idMath::Fabs( nodes[i].origin.z - z ) <= NAV_MERGE_HEIGHT ) {
			return i;
		}
	}

	return -1;
}

/*
================
rvNavMesh::AddNode
================
*/
int rvNavMesh::AddNode( int gx, int gy, const idVec3 &origin ) {
	navNode_t node;

	node.origin		= origin;
	node.gx			= gx;
	node.gy			= gy;
	node.firstLink	= -1;
	node.area		= -1;

	const int index = nodes.Append( node );
	nodeHash.Add( nodeHash.GenerateKey( gx, gy ), index );

	return index;
}

/*
================
rvNavMesh::LinkNodes
================
*/
void rvNavMesh::LinkNodes( int from, int to, float cost, int travelType, int actionEntityNum ) {
	navLink_t link;
	const float geometricCost = ( nodes[to].origin - nodes[from].origin ).Length();
	const float edgeScale = ( geometricCost > idMath::FLOAT_EPSILON )
		? Max( 0.0f, cost / geometricCost )
		: 1.0f;

	// Do not duplicate a link that already exists with a cheaper cost.
	for ( int i = nodes[from].firstLink; i != -1; i = links[i].next ) {
		if ( links[i].node == to ) {
			if ( cost < links[i].cost ) {
				links[i].cost				= cost;
				links[i].travelType			= (short)travelType;
				links[i].actionEntityNum	= (short)actionEntityNum;
				heuristicScale = Min( heuristicScale, edgeScale );
			}
			return;
		}
	}

	link.node				= to;
	link.next				= nodes[from].firstLink;
	link.cost				= cost;
	link.travelType			= (short)travelType;
	link.actionEntityNum	= (short)actionEntityNum;

	nodes[from].firstLink = links.Append( link );
	heuristicScale = Min( heuristicScale, edgeScale );
}

/*
================
rvNavMesh::SampleFloor

Drops the agent box straight down and reports where it comes to rest.  Fails on
a pit deeper than maxDrop and on anything too steep to stand on.
================
*/
bool rvNavMesh::SampleFloor( const idVec3 &start, float maxDrop, idVec3 &floorOut ) const {
	trace_t	trace;
	idVec3	end;

	end = start;
	end.z -= maxDrop;

	NavTranslationThroughDoors( trace, start, end, navAgentModel );

	if ( trace.fraction >= 1.0f ) {
		return false;
	}

	// A sweep that begins inside solid also reports fraction 0, with endpos at
	// the start and whatever normal the first plane happened to have.  Callers
	// always probe from at least NAV_GROUND_OFFSET above a surface, so a zero
	// fraction here means the probe point itself was solid, not that it landed.
	if ( trace.fraction <= 0.0f ) {
		return false;
	}

	if ( trace.c.normal.z < NAV_WALKABLE_NORMAL_Z ) {
		return false;
	}

	floorOut = trace.endpos;

	return true;
}

/*
================
rvNavMesh::TryStep

Reproduces one step of player movement into the neighbouring cell: try to walk
there flat, and if that is blocked try again from step height, exactly as
idPhysics_Player does when it climbs a stair.  Whatever the horizontal move
reaches is then dropped onto the floor.
================
*/
bool rvNavMesh::TryStep( const idVec3 &fromFloor, const idVec3 &toCentre, idVec3 &toFloor, int &travelType ) const {
	const float	stepSize = pm_stepsize.GetFloat();
	const float	jumpSize = pm_jumpheight.GetFloat();

	// Try the cheapest traversal that works: walk flat, step up, then jump up.
	// The lift heights are the same ones idPhysics_Player will allow.
	const float lifts[3] = { NAV_GROUND_OFFSET, NAV_GROUND_OFFSET + stepSize, NAV_GROUND_OFFSET + jumpSize };

	for ( int attempt = 0; attempt < 3; attempt++ ) {
		trace_t	trace;
		idVec3	start = fromFloor;
		idVec3	end;

		start.z += NAV_GROUND_OFFSET;

		// Clear headroom for the lift before moving sideways at that height.
		if ( attempt > 0 ) {
			end = start;
			end.z = fromFloor.z + lifts[attempt];

			if ( !NavTranslationThroughDoors( trace, start, end, navAgentModel ) ) {
				return false;
			}
			start = trace.endpos;
		}

		end = toCentre;
		end.z = start.z;

		if ( !NavTranslationThroughDoors( trace, start, end, navAgentModel ) ) {
			continue;
		}

		// Land on whatever is under the cell centre.
		if ( !SampleFloor( trace.endpos, lifts[attempt] + NAV_MAX_DROP, toFloor ) ) {
			continue;
		}

		const float rise = toFloor.z - fromFloor.z;
		if ( rise > jumpSize ) {
			continue;
		}

		if ( attempt == 2 ) {
			// Only the jump-height lift got through, so the bot has to jump
			// whatever the destination floor turns out to be.  A railing it has
			// to hop over lands it back at the same height, and calling that a
			// walk would leave the bot pressed against the railing forever.
			travelType = NAVTRAVEL_JUMP;
		} else if ( rise > stepSize ) {
			// Reachable only by jumping, but that is not what was tried here.
			continue;
		} else {
			travelType = ( fromFloor.z - toFloor.z > stepSize ) ? NAVTRAVEL_DROP : NAVTRAVEL_WALK;
		}

		return true;
	}

	return false;
}

/*
================
rvNavMesh::GatherSeeds

Seeds are places the map itself guarantees a player can stand: spawn points,
items, jump pad landings and teleport exits.  Flooding out from those covers
the playable space without probing the solid two thirds of the world bounds.
================
*/
void rvNavMesh::GatherSeeds( idList<idVec3> &seeds ) const {
	idEntity *ent;

	for ( ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		if ( ent->IsType( idPlayerStart::GetClassType() ) || ent->IsType( idItem::GetClassType() ) ) {
			seeds.Append( ent->GetPhysics()->GetOrigin() );
			continue;
		}

		// A jump pad launches into space; its target is where a player lands.
		if ( ent->IsType( rvJumpPad::GetClassType() ) ) {
			idBounds padBounds;
			if ( NavTravelVolumeBounds( ent, padBounds ) ) {
				seeds.Append( padBounds.GetCenter() );
			}
			for ( int i = 0; i < ent->targets.Num(); i++ ) {
				if ( ent->targets[i].GetEntity() ) {
					seeds.Append( ent->targets[i].GetEntity()->GetPhysics()->GetOrigin() );
				}
			}
		}
	}

	// Anyone already in the map is standing somewhere legal.
	for ( int i = 0; i < gameLocal.numClients; i++ ) {
		idPlayer *player = gameLocal.GetClientByNum( i );
		if ( player && !player->spectating ) {
			seeds.Append( player->GetPhysics()->GetOrigin() );
		}
	}
}

/*
================
rvNavMesh::FloodFill
================
*/
void rvNavMesh::FloodFill( const idList<idVec3> &seeds ) {
	idList<int>	openList;
	int			i;

	openList.SetGranularity( 1024 );

	for ( i = 0; i < seeds.Num(); i++ ) {
		idVec3 probe = seeds[i];
		idVec3 floor;

		// Seeds sit a little above the floor, and items hover.  Look up a touch
		// first so a seed embedded in the surface still resolves.
		probe.z += pm_stepsize.GetFloat();

		if ( !SampleFloor( probe, 256.0f, floor ) ) {
			continue;
		}

		int gx, gy;
		CellCoords( floor, gx, gy );

		// Re-drop at the cell centre so the node is where the grid says it is.
		idVec3 centre = CellCentre( gx, gy, floor.z + pm_stepsize.GetFloat() );
		idVec3 snapped;
		if ( !SampleFloor( centre, pm_stepsize.GetFloat() + NAV_MAX_DROP, snapped ) ) {
			continue;
		}

		if ( FindNode( gx, gy, snapped.z ) != -1 ) {
			continue;
		}

// openQ4 BEGIN
		if ( NavPointInLethalLiquid( snapped ) ) {
			continue;
		}
// openQ4 END

		openList.Append( AddNode( gx, gy, snapped ) );
	}

	// Breadth first so the mesh grows evenly and the open list stays small.
	for ( i = 0; i < openList.Num(); i++ ) {
		const int current = openList[i];

		if ( nodes.Num() >= NAV_MAX_NODES ) {
			gameLocal.Warning( "rvNavMesh: node limit (%d) reached, navmesh is truncated", NAV_MAX_NODES );
			break;
		}

		// Copied, because AddNode below can reallocate the node list.
		const idVec3	fromFloor	= nodes[current].origin;
		const int		fromGX		= nodes[current].gx;
		const int		fromGY		= nodes[current].gy;

		for ( int dir = 0; dir < 8; dir++ ) {
			const int gx = fromGX + navNeighbourX[dir];
			const int gy = fromGY + navNeighbourY[dir];

			idVec3	toFloor;
			int		travelType;

			if ( !TryStep( fromFloor, CellCentre( gx, gy, fromFloor.z ), toFloor, travelType ) ) {
				continue;
			}

			int neighbour = FindNode( gx, gy, toFloor.z );
			if ( neighbour == -1 ) {
// openQ4 BEGIN
				if ( NavPointInLethalLiquid( toFloor ) ) {
					continue;
				}
// openQ4 END
				neighbour = AddNode( gx, gy, toFloor );
				openList.Append( neighbour );
			}

			float cost = ( toFloor - fromFloor ).Length();
			if ( travelType == NAVTRAVEL_DROP ) {
				cost += NAV_DROP_PENALTY;
			} else if ( travelType == NAVTRAVEL_JUMP ) {
				cost += NAV_JUMP_PENALTY;
			}

			LinkNodes( current, neighbour, cost, travelType );

			// A drop is one way unless the reverse step is walkable on its own,
			// which the neighbour discovers when it is expanded.
		}
	}
}

/*
================
rvNavMesh::NodeForEntityPoint

Snaps an entity's world position onto the mesh.  Entities that matter here -
jump pads, teleport exits - are placed by mappers with a generous margin, so a
wide search radius is the right trade.
================
*/
int rvNavMesh::NodeForEntityPoint( const idVec3 &point ) const {
	idVec3 floor;
	idVec3 probe = point;

	probe.z += pm_stepsize.GetFloat();

	if ( SampleFloor( probe, 512.0f, floor ) ) {
		const int node = FindNearestNode( floor, 128.0f );
		if ( node != -1 ) {
			return node;
		}
	}

	return FindNearestNode( point, 256.0f );
}

/*
================
rvNavMesh::AddOffMeshLinks

Jump pads and teleporters move a player between places the walking flood fill
can never connect.  Both are "walk into the volume and you are carried", so
they become one way links whose entry point is the volume itself.
================
*/
void rvNavMesh::AddOffMeshLinks( void ) {
	idEntity *ent;
	int		 numLinksAdded = 0;

	for ( ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		int		travelType;
		float	cost;

		if ( ent->IsType( rvJumpPad::GetClassType() ) ) {
			travelType	= NAVTRAVEL_JUMPPAD;
			cost		= NAV_JUMPPAD_COST;
		} else if ( ent->IsType( idTrigger_Multi::GetClassType() ) ) {
			// idTrigger_Multi doubles as the teleporter trigger: a single
			// target, and that target is a player start.
			if ( ent->targets.Num() != 1 || !ent->targets[0].GetEntity() ) {
				continue;
			}
			if ( !ent->targets[0].GetEntity()->IsType( idPlayerStart::GetClassType() ) ) {
				continue;
			}
			travelType	= NAVTRAVEL_TELEPORT;
			cost		= NAV_TELEPORT_COST;
		} else {
			continue;
		}

		if ( ent->targets.Num() < 1 || !ent->targets[0].GetEntity() ) {
			continue;
		}

		idBounds entryBounds;
		if ( !NavTravelVolumeBounds( ent, entryBounds ) ) {
			continue;
		}

		const int from = NodeForEntityPoint( entryBounds.GetCenter() );
		const int to   = NodeForEntityPoint( ent->targets[0].GetEntity()->GetPhysics()->GetOrigin() );

		if ( from == -1 || to == -1 || from == to ) {
			continue;
		}

		LinkNodes( from, to, cost, travelType, ent->entityNumber );
		numLinksAdded++;
	}

	if ( bot_debug.GetBool() ) {
		gameLocal.Printf( "rvNavMesh: %d off-mesh links\n", numLinksAdded );
	}
}

/*
================
rvNavMesh::BuildAreas

Connected components over the links treated as undirected.  Two nodes in
different components definitely cannot reach each other, which lets a bot
reject an unreachable goal without paying for a full search.  The converse is
not guaranteed - a one way drop puts both ends in the same component - so this
is only ever used to say no.
================
*/
void rvNavMesh::BuildAreas( void ) {
	idList<int>	stack;
	int			i;

	// Reverse adjacency, so an undirected walk can follow one way links back.
	idList<int> reverseHead;
	idList<int> reverseNext;
	idList<int> reverseNode;

	reverseHead.AssureSize( nodes.Num(), -1 );
	reverseNext.SetGranularity( 4096 );
	reverseNode.SetGranularity( 4096 );

	for ( i = 0; i < nodes.Num(); i++ ) {
		for ( int l = nodes[i].firstLink; l != -1; l = links[l].next ) {
			const int dest = links[l].node;
			reverseNode.Append( i );
			reverseNext.Append( reverseHead[dest] );
			reverseHead[dest] = reverseNode.Num() - 1;
		}
	}

	numAreas = 0;

	for ( i = 0; i < nodes.Num(); i++ ) {
		if ( nodes[i].area != -1 ) {
			continue;
		}

		const int area = numAreas++;

		stack.Clear();
		stack.Append( i );
		nodes[i].area = area;

		while ( stack.Num() ) {
			const int current = stack[stack.Num() - 1];
			stack.SetNum( stack.Num() - 1, false );

			for ( int l = nodes[current].firstLink; l != -1; l = links[l].next ) {
				if ( nodes[links[l].node].area == -1 ) {
					nodes[links[l].node].area = area;
					stack.Append( links[l].node );
				}
			}

			for ( int r = reverseHead[current]; r != -1; r = reverseNext[r] ) {
				if ( nodes[reverseNode[r]].area == -1 ) {
					nodes[reverseNode[r]].area = area;
					stack.Append( reverseNode[r] );
				}
			}
		}
	}
}

/*
================
rvNavMesh::Build
================
*/
bool rvNavMesh::Build( void ) {
	idList<idVec3>	seeds;
	const int		startMsec = Sys_Milliseconds();

	// There is no collision world to probe until a map is running, and
	// GetWorldBounds would index an empty clip world list.
	if ( gameLocal.GameState() != GAMESTATE_ACTIVE ) {
		gameLocal.Warning( "rvNavMesh: no map is running, cannot build a navmesh" );
		return false;
	}

	Clear();

	cellSize = bot_navCellSize.GetFloat();
	if ( cellSize < 8.0f ) {
		cellSize = 8.0f;
	}

	gridOrigin = gameLocal.GetWorldBounds( NULL )[0];

	navAgentModel = new idClipModel( idTraceModel( GetAgentBounds() ) );

	seeds.SetGranularity( 256 );
	GatherSeeds( seeds );

	if ( seeds.Num() == 0 ) {
		gameLocal.Warning( "rvNavMesh: no spawn points or items to seed from, cannot build a navmesh" );
		Clear();
		return false;
	}

	FloodFill( seeds );

	if ( nodes.Num() == 0 ) {
		gameLocal.Warning( "rvNavMesh: no walkable ground found from %d seeds", seeds.Num() );
		Clear();
		return false;
	}

	AddOffMeshLinks();
	BuildAreas();

	// The graph is fixed from here on, so the search scratch can be sized once
	// instead of on every query.
	gScore.SetNum( nodes.Num(), true );
	fScore.SetNum( nodes.Num(), true );
	cameFrom.SetNum( nodes.Num(), true );
	openTag.SetNum( nodes.Num(), true );
	closedTag.SetNum( nodes.Num(), true );
	heap.Resize( 1024, 1024 );

	for ( int i = 0; i < nodes.Num(); i++ ) {
		openTag[i]	 = 0;
		closedTag[i] = 0;
	}
	searchSerial = 0;

	// The probe model is only needed for generation; routing traces reuse it,
	// so it stays alive until Clear().
	buildMsec = Sys_Milliseconds() - startMsec;

	gameLocal.Printf( "rvNavMesh: %d nodes, %d links, %d areas, %d seeds in %d ms (cell %g)\n",
					  nodes.Num(), links.Num(), numAreas, seeds.Num(), buildMsec, cellSize );

	return true;
}

/*
===============================================================================

	Queries

===============================================================================
*/

/*
================
rvNavMesh::FindNearestNode

Searches the grid cells around the point outwards, so the common case - the
point is standing on a node - costs one cell lookup.
================
*/
int rvNavMesh::FindNearestNode( const idVec3 &origin, float maxRadius, bool requireWalkable ) const {
	if ( nodes.Num() == 0 ) {
		return -1;
	}

	int gx, gy;
	CellCoords( origin, gx, gy );

	const int	maxRings	= (int)idMath::Ceil( maxRadius / cellSize );
	int			best		= -1;
	float		bestDistSqr = maxRadius * maxRadius;

	for ( int ring = 0; ring <= maxRings; ring++ ) {
		for ( int dy = -ring; dy <= ring; dy++ ) {
			// Step across the shell of this ring rather than walking the whole
			// square and skipping the interior: the interior was covered by the
			// rings before it, and iterating it anyway turns an O(rings^2) job
			// into an O(rings^3) one.  That matters because the off-mesh
			// recovery search uses a large radius.
			const bool	edgeRow	= ( abs( dy ) == ring );
			const int	step	= ( edgeRow || ring == 0 ) ? 1 : ( ring * 2 );

			for ( int dx = -ring; dx <= ring; dx += step ) {
				const int key = nodeHash.GenerateKey( gx + dx, gy + dy );
				for ( int i = nodeHash.First( key ); i != -1; i = nodeHash.Next( i ) ) {
					if ( nodes[i].gx != gx + dx || nodes[i].gy != gy + dy ) {
						continue;
					}

					const float distSqr = ( nodes[i].origin - origin ).LengthSqr();
					if ( distSqr < bestDistSqr &&
						 ( !requireWalkable || WalkableLine( origin, nodes[i].origin ) ) ) {
						bestDistSqr	= distSqr;
						best		= i;
					}
				}
			}
		}

		// A hit one ring in cannot be beaten by anything two rings out.
		if ( best != -1 && bestDistSqr <= ( ring * cellSize ) * ( ring * cellSize ) ) {
			break;
		}
	}

	return best;
}

/*
================
rvNavMesh::IsReachable
================
*/
bool rvNavMesh::IsReachable( const idVec3 &start, const idVec3 &goal ) const {
	rvNavPath path;
	return FindPath( start, goal, path );
}

/*
================
rvNavMesh::RandomReachablePoint
================
*/
bool rvNavMesh::RandomReachablePoint( const idVec3 &origin, idVec3 &result, rvNavPath *pathOut ) const {
	if ( pathOut ) {
		pathOut->Clear();
	}

	if ( nodes.Num() == 0 ) {
		return false;
	}

	const int startNode = FindNearestNode( origin, 256.0f, true );
	if ( startNode == -1 ) {
		return false;
	}

	// Build the directed closure once, then choose uniformly from it.  Weak
	// areas intentionally treat a one-way drop as undirected, while a handful
	// of random probes can miss the small set that is truly reachable from a
	// ledge or island.  Following outgoing links is both exact and linear.
	BeginSearch();

	idList<int> reachable;
	reachable.SetGranularity( 256 );
	reachable.Append( startNode );
	openTag[startNode] = searchSerial;

	for ( int cursor = 0; cursor < reachable.Num(); cursor++ ) {
		const int node = reachable[cursor];

		for ( int l = nodes[node].firstLink; l != -1; l = links[l].next ) {
			const int destination = links[l].node;
			if ( openTag[destination] == searchSerial ) {
				continue;
			}

			openTag[destination] = searchSerial;
			reachable.Append( destination );
		}
	}

	if ( reachable.Num() <= 1 ) {
		return false;
	}

	// The directed closure reflects the generated graph, while movers can make
	// one branch temporarily unavailable.  Sample without replacement so one
	// obstructed random pick does not cancel roaming when many others work.
	const int maxAttempts = Min( NAV_MAX_ROUTE_REPAIRS, reachable.Num() - 1 );
	for ( int attempt = 0; attempt < maxAttempts; attempt++ ) {
		const int candidateIndex = 1 + gameLocal.random.RandomInt( reachable.Num() - 1 );
		const int candidate = reachable[candidateIndex];
		reachable.RemoveIndex( candidateIndex );

		rvNavPath candidatePath;
		if ( !FindPath( origin, nodes[candidate].origin, candidatePath ) ) {
			continue;
		}

		result = nodes[candidate].origin;
		if ( pathOut ) {
			*pathOut = candidatePath;
		}
		return true;
	}

	return false;
}

/*
================
rvNavMesh::WalkableLine

String-pull test.  A straight box sweep proves nothing is in the way, but it
would also happily cut the corner across a pit, so the ground underneath is
sampled as well.
================
*/
bool rvNavMesh::WalkableLine( const idVec3 &from, const idVec3 &to ) const {
	trace_t		trace;
	const float	stepSize = pm_stepsize.GetFloat();

	idVec3 start = from + idVec3( 0.0f, 0.0f, NAV_GROUND_OFFSET );
	idVec3 end   = to   + idVec3( 0.0f, 0.0f, NAV_GROUND_OFFSET );
	float probeLift = NAV_GROUND_OFFSET;

	// Most shortcuts are ordinary floor movement.  Test at standing height
	// first so a legal low corridor is not rejected merely because a
	// step-height-raised player hull would overlap its ceiling.
	if ( !NavTranslationThroughDoors( trace, start, end, navAgentModel ) ) {
		// A flat sweep can hit a stair that player physics will step over.  The
		// raised alternative is only needed in that case.
		start = from + idVec3( 0.0f, 0.0f, stepSize );
		end   = to   + idVec3( 0.0f, 0.0f, stepSize );
		probeLift = stepSize;

		if ( !NavTranslationThroughDoors( trace, start, end, navAgentModel ) ) {
			return false;
		}
	}

	const idVec3	delta	= to - from;
	const float		length	= delta.Length();
	const int		samples	= ( length > idMath::FLOAT_EPSILON )
								? Max( 2, (int)idMath::Ceil( length / cellSize ) )
								: 1;

	for ( int i = 1; i < samples; i++ ) {
		const float	frac  = (float)i / (float)samples;
		const idVec3	expected = from + delta * frac;
		idVec3		probe = expected + idVec3( 0.0f, 0.0f, probeLift );
		idVec3		floor;

		if ( !SampleFloor( probe, probeLift + NAV_SMOOTH_MAX_DIP, floor ) ) {
			return false;
		}

		// The straight line must stay close to the ground it crosses, or the
		// shortcut is really a fall.
		if ( expected.z - floor.z > NAV_SMOOTH_MAX_DIP ) {
			return false;
		}
	}

	return true;
}

/*
================
rvNavMesh::FindLink
================
*/
int rvNavMesh::FindLink( int fromNode, int toNode ) const {
	if ( fromNode < 0 || fromNode >= nodes.Num() ) {
		return -1;
	}

	for ( int l = nodes[fromNode].firstLink; l != -1; l = links[l].next ) {
		if ( links[l].node == toNode ) {
			return l;
		}
	}

	return -1;
}

/*
================
rvNavMesh::LinkTraversable

Generation records the map's topology, but solid movers can change after the
mesh is built.  Validate only the links on a selected route (rather than every
edge A* examines) and let the caller exclude any stale link before searching
again.  Ordinary doors remain usable because approaching them activates them.
================
*/
bool rvNavMesh::LinkTraversable( int fromNode, int linkIndex ) const {
	if ( fromNode < 0 || fromNode >= nodes.Num() ||
		 linkIndex < 0 || linkIndex >= links.Num() ) {
		return false;
	}

	const navLink_t &link = links[linkIndex];
	if ( link.node < 0 || link.node >= nodes.Num() ) {
		return false;
	}

	switch ( link.travelType ) {
		case NAVTRAVEL_WALK:
		case NAVTRAVEL_DROP:
		case NAVTRAVEL_JUMP: {
			idVec3 start = nodes[fromNode].origin;
			idVec3 end = nodes[link.node].origin;
			trace_t trace;

			start.z += NAV_GROUND_OFFSET;
			end.z += NAV_GROUND_OFFSET;

			// The static world and traversal type were proven when this graph edge
			// was generated.  Only dynamic solid entities can make that proof stale.
			return NavEntityTranslationThroughDoors( trace, start, end, navAgentModel );
		}

		case NAVTRAVEL_JUMPPAD:
		case NAVTRAVEL_TELEPORT: {
			if ( link.actionEntityNum < 0 || link.actionEntityNum >= MAX_GENTITIES ) {
				return false;
			}

			idEntity *action = gameLocal.entities[link.actionEntityNum];
			if ( !action || action->targets.Num() < 1 || !action->targets[0].GetEntity() ) {
				return false;
			}

			if ( link.travelType == NAVTRAVEL_JUMPPAD ) {
				if ( !action->IsType( rvJumpPad::GetClassType() ) ||
					 !action->IsActive( TH_THINK ) ) {
					return false;
				}
			} else if ( !action->IsType( idTrigger_Multi::GetClassType() ) ||
						!action->targets[0].GetEntity()->IsType( idPlayerStart::GetClassType() ) ) {
				return false;
			}

			idBounds activationBounds;
			return NavTravelVolumeBounds( action, activationBounds );
		}

		default:
			return false;
	}
}

/*
================
rvNavMesh::LinkTravelType

How a bot has to get from one node to the next.  Nodes that are not directly
linked - the result of a string pull - are walked.
================
*/
int rvNavMesh::LinkTravelType( int fromNode, int toNode ) const {
	const int link = FindLink( fromNode, toNode );
	return ( link != -1 ) ? links[link].travelType : NAVTRAVEL_WALK;
}

/*
================
rvNavMesh::LinkActionEntity

Volume traversal has to retain the pad or trigger that owns the graph edge.
The bot uses its live bounds to enter the volume before steering toward the
remote exit.
================
*/
int rvNavMesh::LinkActionEntity( int fromNode, int toNode ) const {
	const int link = FindLink( fromNode, toNode );
	return ( link != -1 ) ? links[link].actionEntityNum : -1;
}

/*
================
rvNavMesh::StringPull

Turns the cell-by-cell node chain into the small number of corners a bot
actually has to steer through.  Only walk links are ever skipped: a jump pad or
a teleporter has to be entered deliberately, so those corners always survive.
================
*/
void rvNavMesh::StringPull( const idList<int> &nodePath, const idVec3 &goal, rvNavPath &path ) const {
	int i = 0;

	path.Clear();

	while ( i < nodePath.Num() - 1 ) {
		// How far ahead smoothing is even allowed to look: never past a link
		// that has to be travelled deliberately.
		int limit = Min( i + NAV_SMOOTH_LOOKAHEAD, nodePath.Num() - 1 );
		int immediateTravel = NAVTRAVEL_WALK;

		for ( int j = i + 1; j <= limit; j++ ) {
			const int travelType = LinkTravelType( nodePath[j - 1], nodePath[j] );
			if ( travelType != NAVTRAVEL_WALK ) {
				limit = j - 1;
				if ( j == i + 1 ) {
					immediateTravel = travelType;
				}
				break;
			}
		}

		// A mature reachability carries both its entry and exit.  If the route
		// begins with an action (or resumes immediately after one), preserve the
		// source explicitly before its destination.  The bot can then align
		// with a jump takeoff or enter a pad/teleporter volume instead of aiming
		// directly at a remote endpoint.
		if ( immediateTravel != NAVTRAVEL_WALK ) {
			const idVec3 &source = nodes[nodePath[i]].origin;
			if ( path.Num() == 0 ||
				 ( path[path.Num() - 1].origin - source ).LengthSqr() > 1.0f ) {
				path.Append( source, NAVTRAVEL_WALK );
			}

			path.Append( nodes[nodePath[i + 1]].origin, immediateTravel,
						 LinkActionEntity( nodePath[i], nodePath[i + 1] ) );
			i++;
			continue;
		}

		// Walk forward while the shortcut still holds.  Stopping at the first
		// failure keeps this to a handful of traces per corner.
		int furthest = i + 1;
		for ( int j = i + 2; j <= limit; j++ ) {
			if ( !WalkableLine( nodes[nodePath[i]].origin, nodes[nodePath[j]].origin ) ) {
				break;
			}
			furthest = j;
		}

		path.Append( nodes[nodePath[furthest]].origin, LinkTravelType( nodePath[furthest - 1], nodePath[furthest] ) );
		i = furthest;
	}

	// Finish at the caller's actual goal rather than the node under it, but
	// only when walking straight there is safe.  The old empty-path shortcut
	// skipped this proof when start and goal snapped to the same node, which
	// could invent a walk through a wall or between stacked floors.
	const idVec3 &lastOrigin = ( path.Num() > 0 )
		? path[path.Num() - 1].origin
		: nodes[nodePath[0]].origin;

	if ( WalkableLine( lastOrigin, goal ) ) {
		path.Append( goal, NAVTRAVEL_WALK );
	}
}

/*
================
rvNavMesh::BeginSearch

The serial-tagged scratch arrays avoid clearing tens of thousands of entries
for every query.  Reset them explicitly before the signed serial can wrap;
otherwise a very long-running server could inherit stale open/closed tags.
================
*/
void rvNavMesh::BeginSearch( void ) const {
	if ( searchSerial >= 0x7ffffffe ) {
		for ( int i = 0; i < openTag.Num(); i++ ) {
			openTag[i] = 0;
			closedTag[i] = 0;
		}
		searchSerial = 1;
	} else {
		searchSerial++;
	}
}

/*
================
rvNavMesh::HeapPush
================
*/
void rvNavMesh::HeapPush( int node, float key ) const {
	navHeapEntry_t entry;

	entry.node	= node;
	entry.key	= key;

	int child = heap.Append( entry );

	while ( child > 0 ) {
		const int parent = ( child - 1 ) >> 1;

		if ( heap[parent].key <= heap[child].key ) {
			break;
		}

		const navHeapEntry_t swap = heap[parent];
		heap[parent] = heap[child];
		heap[child]  = swap;
		child = parent;
	}
}

/*
================
rvNavMesh::HeapPop
================
*/
int rvNavMesh::HeapPop( void ) const {
	const int top = heap[0].node;

	heap[0] = heap[heap.Num() - 1];
	heap.SetNum( heap.Num() - 1, false );

	int parent = 0;
	while ( true ) {
		const int left  = parent * 2 + 1;
		const int right = left + 1;
		int		  best  = parent;

		if ( left < heap.Num() && heap[left].key < heap[best].key ) {
			best = left;
		}
		if ( right < heap.Num() && heap[right].key < heap[best].key ) {
			best = right;
		}
		if ( best == parent ) {
			break;
		}

		const navHeapEntry_t swap = heap[parent];
		heap[parent] = heap[best];
		heap[best]   = swap;
		parent = best;
	}

	return top;
}

/*
================
rvNavMesh::FindPath

A* uses Euclidean distance scaled by the cheapest cost-to-distance ratio in the
graph.  This remains admissible and consistent for jump pads and teleporters,
without making every search on those maps a wide Dijkstra pass.

The generated graph describes topology, not a promise that every mover remains
where it was at build time.  After A* selects a route, only that small set of
edges is checked against live collision.  Blocked edges are excluded and A* is
retried, allowing a bot to choose another corridor without tracing every link
the search merely considered.
================
*/
bool rvNavMesh::FindPath( const idVec3 &start, const idVec3 &goal, rvNavPath &path ) const {
	path.Clear();

	if ( nodes.Num() == 0 ) {
		return false;
	}

	idVec3 routeStart = start;
	idVec3 routeGoal = goal;

	// A player can legitimately request a route while airborne.  Preserve the
	// strict endpoint proof for grounded items and objectives, but route an
	// endpoint that is clearly more than one step above standable ground from or
	// to that landing point.  A grounded endpoint starts its hull sweep touching
	// the floor and therefore cannot project through it onto a stacked level.
	idVec3 projected;
	if ( SampleFloor( start, NAV_MAX_DROP, projected ) &&
		 start.z - projected.z > pm_stepsize.GetFloat() ) {
		routeStart = projected;
	}
	if ( SampleFloor( goal, NAV_MAX_DROP, projected ) &&
		 goal.z - projected.z > pm_stepsize.GetFloat() ) {
		routeGoal = projected;
	}

	const int startNode = FindNearestNode( routeStart, 256.0f, true );
	const int goalNode  = FindNearestNode( routeGoal, 256.0f, true );

	if ( startNode == -1 || goalNode == -1 ) {
		return false;
	}
	if ( nodes[startNode].area != nodes[goalNode].area ) {
		return false;
	}

	if ( startNode == goalNode ) {
		// Same cell, but "same cell" is not the same as "nothing in the way" -
		// a pillar can sit between the two exact points.  Both endpoints snapped
		// to this node through individually walkable legs, so use the node as a
		// real corner when the direct shortcut is blocked.  Recheck the outbound
		// direction because WalkableLine's floor sampling is directional.
		if ( !WalkableLine( routeStart, routeGoal ) ) {
			const idVec3 &via = nodes[startNode].origin;
			if ( !WalkableLine( routeStart, via ) || !WalkableLine( via, routeGoal ) ) {
				return false;
			}

			if ( ( via - routeStart ).LengthSqr() > 1.0f &&
				 ( via - routeGoal ).LengthSqr() > 1.0f ) {
				path.Append( via, NAVTRAVEL_WALK );
			}
		}

		if ( path.Num() == 0 ||
			 ( path[path.Num() - 1].origin - routeGoal ).LengthSqr() > 1.0f ) {
			path.Append( routeGoal, NAVTRAVEL_WALK );
		}

		const float exactCost = path.LengthFrom( start );
		path.SetTravelCost( start, exactCost );
		return path.Num() > 0;
	}

	const idVec3 goalOrigin = nodes[goalNode].origin;
	idList<int> rejectedLinks;
	rejectedLinks.SetGranularity( 16 );

	for ( int repair = 0; repair <= NAV_MAX_ROUTE_REPAIRS; repair++ ) {
		BeginSearch();
		heap.SetNum( 0, false );

		gScore[startNode]	= 0.0f;
		fScore[startNode]	= heuristicScale *
			( goalOrigin - nodes[startNode].origin ).Length();
		cameFrom[startNode]	= -1;
		openTag[startNode]	= searchSerial;

		HeapPush( startNode, fScore[startNode] );

		bool found = false;
		while ( heap.Num() ) {
			const int current = HeapPop();

			// A stale copy of a node that has already been expanded.
			if ( closedTag[current] == searchSerial ) {
				continue;
			}
			closedTag[current] = searchSerial;

			if ( current == goalNode ) {
				found = true;
				break;
			}

			for ( int l = nodes[current].firstLink; l != -1; l = links[l].next ) {
				bool rejected = false;
				for ( int r = 0; r < rejectedLinks.Num(); r++ ) {
					if ( rejectedLinks[r] == l ) {
						rejected = true;
						break;
					}
				}
				if ( rejected ) {
					continue;
				}

				const int next = links[l].node;
				const float tentative = gScore[current] + links[l].cost;

				if ( openTag[next] == searchSerial && tentative >= gScore[next] ) {
					continue;
				}

				openTag[next]	= searchSerial;
				gScore[next]	= tentative;
				fScore[next]	= tentative + heuristicScale *
					( goalOrigin - nodes[next].origin ).Length();
				cameFrom[next]	= current;

				HeapPush( next, fScore[next] );
			}
		}

		if ( !found ) {
			path.Clear();
			return false;
		}

		// Unwind, then reverse.
		idList<int> nodePath;
		nodePath.SetGranularity( 64 );

		for ( int node = goalNode; node != -1; node = cameFrom[node] ) {
			nodePath.Append( node );
		}

		for ( int i = 0, j = nodePath.Num() - 1; i < j; i++, j-- ) {
			const int swap = nodePath[i];
			nodePath[i] = nodePath[j];
			nodePath[j] = swap;
		}

		// Recheck only the selected route against the current collision world.
		// Add every stale edge found in this pass, so wide blockers usually need
		// one repair search rather than one search per blocked cell.
		bool routeValid = true;
		for ( int i = 0; i + 1 < nodePath.Num(); i++ ) {
			const int link = FindLink( nodePath[i], nodePath[i + 1] );
			if ( link == -1 ) {
				path.Clear();
				return false;
			}
			if ( LinkTraversable( nodePath[i], link ) ) {
				continue;
			}

			rejectedLinks.Append( link );
			routeValid = false;
		}

		if ( !routeValid ) {
			continue;
		}

		const float exactTravelCost =
			( routeStart - start ).Length() +
			( nodes[startNode].origin - routeStart ).Length() +
			gScore[goalNode] +
			( routeGoal - nodes[goalNode].origin ).Length();

		StringPull( nodePath, routeGoal, path );

		// The routed start (the exact origin or an airborne landing projection) was
		// proven against startNode when it was selected, but a later string pull can
		// replace that first node with a farther corner.  Two individually clear legs
		// around a wall do not prove their diagonal.  Put the start node back when
		// that final shortcut is not itself clear.
		if ( path.Num() > 0 && !WalkableLine( routeStart, path[0].origin ) ) {
			rvNavPath safeStartPath;
			safeStartPath.Append( nodes[startNode].origin, NAVTRAVEL_WALK );

			for ( int i = 0; i < path.Num(); i++ ) {
				safeStartPath.Append( path[i].origin, path[i].travelType,
									  path[i].actionEntityNum );
			}

			path = safeStartPath;
		}

		// StringPull deliberately refuses an unsafe final shortcut from the last
		// sampled node to the exact goal or projected landing point.  That refusal is
		// a failed route, not a useful partial path: handing it to goal selection
		// makes the bot arrive at the wall or wrong floor and immediately choose the
		// same target again.
		if ( path.Num() == 0 ||
			 ( path[path.Num() - 1].origin - routeGoal ).LengthSqr() > 1.0f ) {
			path.Clear();
			return false;
		}

		path.SetTravelCost( start, exactTravelCost );
		return true;
	}

	path.Clear();
	return false;
}

/*
================
rvNavMesh::DebugDraw
================
*/
void rvNavMesh::DebugDraw( const idVec3 &viewOrigin, float radius ) const {
	static const idVec4 *travelColors[NAVTRAVEL_NUM] = {
		&colorGreen,		// walk
		&colorYellow,		// drop
		&colorOrange,		// jump
		&colorMagenta,		// jump pad
		&colorCyan			// teleport
	};

	// The renderer keeps a fixed debug line pool (MAX_DEBUG_LINES, 16384) and
	// silently drops anything past it, so a dense mesh drawn without a budget
	// fills the pool with whichever nodes happened to come first and shows
	// nothing useful.  Spend the budget near the viewer instead.
	static const int NAV_DEBUG_MAX_LINES = 8192;

	const float	radiusSqr	= radius * radius;
	int			budget		= NAV_DEBUG_MAX_LINES;

	for ( int i = 0; i < nodes.Num() && budget > 0; i++ ) {
		if ( ( nodes[i].origin - viewOrigin ).LengthSqr() > radiusSqr ) {
			continue;
		}

		gameRenderWorld->DebugLine( colorBlue, nodes[i].origin, nodes[i].origin + idVec3( 0.0f, 0.0f, 8.0f ), 0 );
		budget--;

		for ( int l = nodes[i].firstLink; l != -1 && budget > 0; l = links[l].next ) {
			const int type = idMath::ClampInt( 0, NAVTRAVEL_NUM - 1, links[l].travelType );

			// One direction only; the reverse link would draw the same segment.
			if ( links[l].node < i && links[l].travelType == NAVTRAVEL_WALK ) {
				continue;
			}

			gameRenderWorld->DebugLine( *travelColors[type],
										nodes[i].origin + idVec3( 0.0f, 0.0f, 4.0f ),
										nodes[links[l].node].origin + idVec3( 0.0f, 0.0f, 4.0f ),
										0 );
			budget--;
		}
	}
}
