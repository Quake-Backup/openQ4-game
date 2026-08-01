//----------------------------------------------------------------
// NavMesh.h
//
// Runtime navigation mesh for openQ4 multiplayer bots.
//
// Quake 4 never shipped .aas files for any of its multiplayer maps, so the
// SDK's AAS routing has nothing to route over there.  Rather than require a
// per-map offline compile, this builds the navigation graph at map load by
// probing the live collision world with the player's own bounding box, the
// same way a modern navmesh generator voxelises a scene - except the "voxels"
// come from the engine's collision traces, so anything the graph claims is
// walkable is walkable by construction.
//
// The result is a multi-level grid navmesh: one node per (grid cell, floor
// level) pair, linked to its neighbours by traversal edges that record how the
// move has to be made (walk, drop, jump pad, teleporter).  Routing is A* over
// those links, and the resulting corner list is string-pulled against the
// collision world so bots move in straight lines instead of tracing the grid.
//----------------------------------------------------------------

#ifndef __GAME_MP_NAVMESH_H__
#define __GAME_MP_NAVMESH_H__

class idEntity;

//----------------------------------------------------------------
// How a link between two nodes has to be travelled.  The bot's movement code
// switches on this: walking is steering, a drop is a walk off a ledge, and the
// two volume links are "get into the volume and let it carry you".
//----------------------------------------------------------------
typedef enum {
	NAVTRAVEL_WALK,				// flat ground, stairs, ramps
	NAVTRAVEL_DROP,				// step off a ledge, one way
	NAVTRAVEL_JUMP,				// hop up onto a ledge a step cannot reach
	NAVTRAVEL_JUMPPAD,			// walk into an rvJumpPad, one way
	NAVTRAVEL_TELEPORT,			// walk into a teleporter trigger, one way
	NAVTRAVEL_NUM
} navTravelType_t;

//----------------------------------------------------------------
// A single navigation node.  origin sits on the floor, at the centre of its
// grid cell, so it is directly usable as a movement target.
//----------------------------------------------------------------
typedef struct navNode_s {
	idVec3					origin;
	int						gx;				// grid coordinates, for neighbour lookups
	int						gy;
	int						firstLink;		// head of the singly linked outgoing link list, -1 if isolated
	int						area;			// connected component id, for cheap reachability rejection
} navNode_t;

typedef struct navLink_s {
	int						node;			// destination node
	int						next;			// next link out of the same source node, -1 terminates
	float					cost;
	short					travelType;		// navTravelType_t
	short					actionEntityNum;// pad/teleporter that owns this link, -1 otherwise
} navLink_t;

//----------------------------------------------------------------
// One corner of a route.  travelType describes how the bot gets to this
// corner from the previous one, so the movement code knows when it is walking
// and when it has to commit to a jump pad or a teleporter.
//----------------------------------------------------------------
typedef struct navCorner_s {
	idVec3					origin;
	short					travelType;
	short					actionEntityNum;// preserves volume identity through string pulling
} navCorner_t;

class rvNavPath {
public:
							rvNavPath( void ) {
								corners.SetGranularity( 32 );
								travelStart.Zero();
								travelCost = 0.0f;
								hasTravelCost = false;
							}

	void					Clear( void ) {
								corners.Clear();
								travelStart.Zero();
								travelCost = 0.0f;
								hasTravelCost = false;
							}
	int						Num( void ) const { return corners.Num(); }
	bool					IsEmpty( void ) const { return corners.Num() == 0; }
	const navCorner_t &		operator[]( int index ) const { return corners[index]; }

	void					Append( const idVec3 &origin, int travelType, int actionEntityNum = -1 );

	// Total length of the remaining route, used to compare goals.
	float					Length( void ) const;
	// Weighted route cost from the exact origin used for the search.  Falls
	// back to geometric corner length when the caller supplies another origin.
	float					LengthFrom( const idVec3 &origin ) const;

private:
	friend class rvNavMesh;

	void					SetTravelCost( const idVec3 &origin, float cost );

	idList<navCorner_t>		corners;
	idVec3					travelStart;	// exact origin used by the route search
	float					travelCost;		// graph cost, including traversal penalties/shortcuts
	bool					hasTravelCost;
};

//----------------------------------------------------------------
// rvNavMesh
//----------------------------------------------------------------
class rvNavMesh {
public:
							rvNavMesh( void );
							~rvNavMesh( void );

	// Probe the collision world and build the graph.  Safe to call again; the
	// previous graph is thrown away first.  Returns false if nothing walkable
	// was found, which is the "this map cannot host bots" answer.
	bool					Build( void );
	void					Clear( void );

	bool					IsValid( void ) const { return nodes.Num() > 0; }
	int						NumNodes( void ) const { return nodes.Num(); }
	int						NumLinks( void ) const { return links.Num(); }
	int						GetBuildMilliseconds( void ) const { return buildMsec; }

	const navNode_t &		GetNode( int index ) const { return nodes[index]; }

	// Nearest sampled standable node.  Returns -1 when the point is too far
	// from the mesh entirely.  Routing/recovery callers can require a proven
	// walkable connection from the exact point instead of snapping through a
	// thin wall or onto another floor layer.
	int						FindNearestNode( const idVec3 &origin, float maxRadius = 256.0f,
											 bool requireWalkable = false ) const;

	// A* from start to goal.  Both ends are snapped onto the mesh first.  The
	// returned path is string-pulled and ends at the exact goal, except that an
	// airborne endpoint may conservatively end at its projected landing floor.
	bool					FindPath( const idVec3 &start, const idVec3 &goal, rvNavPath &path ) const;

	// Directed "is there any route at all" test.  Weak area equality is used
	// only as an early rejection; one-way drops still require a real search.
	bool					IsReachable( const idVec3 &start, const idVec3 &goal ) const;

	// Random node with a directed route from origin, for roaming.  When
	// pathOut is supplied, the successful search is returned for immediate use.
	bool					RandomReachablePoint( const idVec3 &origin, idVec3 &result,
												 rvNavPath *pathOut = NULL ) const;

	void					DebugDraw( const idVec3 &viewOrigin, float radius ) const;

	static const idBounds &	GetAgentBounds( void );

private:
	// -- generation --
	void					GatherSeeds( idList<idVec3> &seeds ) const;
	void					FloodFill( const idList<idVec3> &seeds );
	void					AddOffMeshLinks( void );
	void					BuildAreas( void );

	bool					SampleFloor( const idVec3 &start, float maxDrop, idVec3 &floorOut ) const;
	bool					TryStep( const idVec3 &fromFloor, const idVec3 &toCentre, idVec3 &toFloor, int &travelType ) const;

	int						AddNode( int gx, int gy, const idVec3 &origin );
	int						FindNode( int gx, int gy, float z ) const;
	void					LinkNodes( int from, int to, float cost, int travelType,
									   int actionEntityNum = -1 );

	// Snap a world point onto the mesh, adding a link entity's landing spot.
	int						NodeForEntityPoint( const idVec3 &point ) const;

	idVec3					CellCentre( int gx, int gy, float z ) const;
	void					CellCoords( const idVec3 &origin, int &gx, int &gy ) const;

	// -- routing --
	bool					WalkableLine( const idVec3 &from, const idVec3 &to ) const;
	bool					LinkTraversable( int fromNode, int linkIndex ) const;
	int						FindLink( int fromNode, int toNode ) const;
	void					StringPull( const idList<int> &nodePath, const idVec3 &goal, rvNavPath &path ) const;
	int						LinkTravelType( int fromNode, int toNode ) const;
	int						LinkActionEntity( int fromNode, int toNode ) const;

	// The heap carries its own ordering key.  Reading fScore[] at compare time
	// would be wrong: relaxing a node already in the heap rewrites its score
	// and would silently reorder entries that are already placed.
	typedef struct navHeapEntry_s {
		int						node;
		float					key;
	} navHeapEntry_t;

	void					HeapPush( int node, float key ) const;
	int						HeapPop( void ) const;
	void					BeginSearch( void ) const;

	idList<navNode_t>		nodes;
	idList<navLink_t>		links;
	idHashIndex				nodeHash;

	idVec3					gridOrigin;
	float					cellSize;
	int						numAreas;

	int						buildMsec;
	// Smallest stored edge-cost/geometric-distance ratio.  Scaling Euclidean
	// distance by this keeps A* admissible even with cheap long-range
	// transports, while retaining more guidance than falling back to Dijkstra.
	float					heuristicScale;

	// Scratch state for the A* pass.  Mutable because routing is
	// logically a const query - the alternative is allocating these on every
	// search, and bots search often.
	mutable idList<float>	gScore;
	mutable idList<float>	fScore;
	mutable idList<int>		cameFrom;
	mutable idList<int>		openTag;		// search serial number, avoids clearing the arrays
	mutable idList<int>		closedTag;
	mutable idList<navHeapEntry_t>	heap;	// binary min-heap, ordered by its own stored key
	mutable int				searchSerial;
};

extern rvNavMesh			navMesh;

#endif /* !__GAME_MP_NAVMESH_H__ */
