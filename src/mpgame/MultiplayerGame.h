// RAVEN BEGIN
// ddynerman: note that this file is no longer merged with Doom3 updates
//
// MERGE_DATE 09/30/2004

#ifndef __MULTIPLAYERGAME_H__
#define	__MULTIPLAYERGAME_H__

/*
===============================================================================

	Quake IV multiplayer

===============================================================================
*/

#include "mp/GameTypeIds.h"
#include "mp/Buying.h"
#include "mp/HitFeedback.h"
#include "mp/match/MatchRules.h"
#include "mp/match/MatchSeries.h"
#include "mp/match/MatchSeriesRecovery.h"
#include "mp/match/MatchSeriesReport.h"
#include "mp/match/MatchSeriesReportStorage.h"
#include "mp/match/MatchSession.h"
#include "mp/match/MatchTeamCommunication.h"
#include "mp/match/MatchTerminationPolicy.h"
#include "mp/match/MatchTeams.h"
#include "mp/match/MatchAuthentication.h"
#include "mp/match/MatchEvidence.h"
#include "mp/match/MatchEvidenceObserver.h"
#include "mp/match/MatchEvidenceStorage.h"
#include "mp/match/MatchDisclosurePolicy.h"
#include "mp/match/MatchItemTiming.h"
#include "mp/match/MatchOperations.h"
#include "mp/match/MatchProposal.h"
#include "mp/match/MatchView.h"
#include "mp/match/MatchControlModel.h"
class idPlayer;
class idItem;
class rvCTF_AssaultPoint;
class rvItemCTFFlag;

// jmarshall - the engine file system interface only exposes map decls by
// index; this resolves a map path the way the old GetMapDecl( name ) did.
const idDict *MultiplayerResolveMapDecl( const char *mapPath );
// shouchard:  server admin command types
typedef enum {
	SERVER_ADMIN_KICK,
	SERVER_ADMIN_BAN,
	SERVER_ADMIN_REMOVE_BAN,
	SERVER_ADMIN_FORCE_SWITCH,
} serverAdmin_t;

// shouchard:  vote struct for packing up interface values to be handled later
//             note that we have two mechanisms for dealing with vote data that
//             should be consolidated:  this one that handles the interface and
//             multi-field votes and the one that handles console commands and
//             single line votes.  
struct voteStruct_t {
	int				m_fieldFlags;	// flags for which fields are valid
	int				m_kick;			// id of the player
	idStr			m_map;			// name of the map
	int				m_gameType;		// game type enum
	int				m_timeLimit;
	int				m_fragLimit;
	int				m_tourneyLimit;
	int				m_captureLimit;
	int				m_buying;
	int				m_teamBalance;
	int				m_controlTime;
	// restart is a flag only
	// nextmap is a flag only but we don't technically support it (but doom had it so it's here)
};

typedef enum {
	VOTEFLAG_RESTART		= 0x0001,
	VOTEFLAG_BUYING			= 0x0002,
	VOTEFLAG_TEAMBALANCE	= 0x0004,
	VOTEFLAG_SHUFFLE		= 0x0008,
	VOTEFLAG_KICK			= 0x0010,
	VOTEFLAG_MAP			= 0x0020,
	VOTEFLAG_GAMETYPE		= 0x0040,
	VOTEFLAG_TIMELIMIT		= 0x0080,
	VOTEFLAG_TOURNEYLIMIT	= 0x0100,
	VOTEFLAG_CAPTURELIMIT	= 0x0200,
	VOTEFLAG_FRAGLIMIT		= 0x0400,
	VOTEFLAG_CONTROLTIME	= 0x0800,
} voteFlag_t;

#define NUM_VOTES			12	// VOTEFLAG_CONTROLTIME is bit 11 and must participate in vote masking
#define VOTEFLAG_ALL			( ( 1 << NUM_VOTES ) - 1 )
#define MAX_PRINT_LEN 128

// more compact than a chat line
typedef enum {
	MSG_SUICIDE = 0,
	MSG_KILLED,
	MSG_KILLEDTEAM,
	MSG_DIED,
	MSG_VOTE,
	MSG_VOTEPASSED,
	MSG_VOTEFAILED,
	MSG_SUDDENDEATH,
	MSG_FORCEREADY,
	MSG_JOINEDSPEC,
	MSG_TIMELIMIT,
	MSG_FRAGLIMIT,
	MSG_CAPTURELIMIT,
	MSG_TELEFRAGGED,
	MSG_JOINTEAM,
	MSG_HOLYSHIT,
	MSG_COUNT
} msg_evt_t;

typedef enum {
	PLAYER_VOTE_NONE,
	PLAYER_VOTE_NO,
	PLAYER_VOTE_YES,
	PLAYER_VOTE_WAIT	// mark a player allowed to vote
} playerVote_t;

typedef enum {
	PRM_AUTO,
	PRM_SCORE,
	PRM_TEAM_SCORE,
	PRM_TEAM_SCORE_PLUS_SCORE,
	PRM_WINS
} playerRankMode_t;

enum announcerSound_t {
	// General announcements
	AS_GENERAL_ONE,
	AS_GENERAL_TWO,
	AS_GENERAL_THREE,
	AS_GENERAL_YOU_WIN,
	AS_GENERAL_YOU_LOSE,
	AS_GENERAL_FIGHT,
	AS_GENERAL_SUDDEN_DEATH,
	AS_GENERAL_VOTE_FAILED,
	AS_GENERAL_VOTE_PASSED,
	AS_GENERAL_VOTE_NOW,
	AS_GENERAL_ONE_FRAG,
	AS_GENERAL_TWO_FRAGS,
	AS_GENERAL_THREE_FRAGS,
	AS_GENERAL_ONE_MINUTE,
	AS_GENERAL_FIVE_MINUTE,
	AS_GENERAL_PREPARE_TO_FIGHT,
	AS_GENERAL_QUAD_DAMAGE,
	AS_GENERAL_REGENERATION,
	AS_GENERAL_HASTE,
	AS_GENERAL_INVISIBILITY,
	// DM announcements
	AS_DM_YOU_TIED_LEAD,
	AS_DM_YOU_HAVE_TAKEN_LEAD,
	AS_DM_YOU_LOST_LEAD,
	// Team announcements
	AS_TEAM_ENEMY_SCORES,
	AS_TEAM_YOU_SCORE,
	AS_TEAM_TEAMS_TIED,
	AS_TEAM_STROGG_LEAD,
	AS_TEAM_MARINES_LEAD,
	AS_TEAM_JOIN_MARINE,
	AS_TEAM_JOIN_STROGG,
	// CTF announcements
	AS_CTF_YOU_HAVE_FLAG,
	AS_CTF_YOUR_TEAM_HAS_FLAG,
	AS_CTF_ENEMY_HAS_FLAG,
	AS_CTF_YOUR_TEAM_DROPS_FLAG,
	AS_CTF_ENEMY_DROPS_FLAG,
	AS_CTF_YOUR_FLAG_RETURNED,
	AS_CTF_ENEMY_RETURNS_FLAG,
	// Tourney announcements
	AS_TOURNEY_ADVANCE,
	AS_TOURNEY_JOIN_ARENA_ONE,
	AS_TOURNEY_JOIN_ARENA_TWO,
	AS_TOURNEY_JOIN_ARENA_THREE,
	AS_TOURNEY_JOIN_ARENA_FOUR,
	AS_TOURNEY_JOIN_ARENA_FIVE,
	AS_TOURNEY_JOIN_ARENA_SIX,
	AS_TOURNEY_JOIN_ARENA_SEVEN,
	AS_TOURNEY_JOIN_ARENA_EIGHT,
	AS_TOURNEY_JOIN_ARENA_WAITING,
	AS_TOURNEY_DONE,
	AS_TOURNEY_START,
	AS_TOURNEY_ELIMINATED,
	AS_TOURNEY_WON,
	AS_TOURNEY_PRELIMS,
	AS_TOURNEY_QUARTER_FINALS,
	AS_TOURNEY_SEMI_FINALS,
	AS_TOURNEY_FINAL_MATCH,
	AS_GENERAL_TEAM_AMMOREGEN,
	AS_GENERAL_TEAM_DOUBLER,
	AS_NUM_SOUNDS
};

// openQ4 BEGIN
// Announcer events for the modes carried over from Quake Live.  Quake 4 ships
// no round, overtime or elimination voice-overs, and openQ4's rule is to run
// on the retail assets, so these are aliases onto stock clips rather than new
// announcerSound_t values.  announcerSoundDefs[] is index-parallel with no
// compile-time check, so aliasing also avoids the easiest way to break it.
// Point these at dedicated shaders if a voice pack ever ships.
const announcerSound_t AS_ROUND_PREPARE			= AS_GENERAL_PREPARE_TO_FIGHT;
const announcerSound_t AS_ROUND_FIGHT			= AS_GENERAL_FIGHT;
const announcerSound_t AS_ROUND_YOU_WIN			= AS_TEAM_YOU_SCORE;
const announcerSound_t AS_ROUND_YOU_LOSE		= AS_TEAM_ENEMY_SCORES;
const announcerSound_t AS_ROUND_DRAW			= AS_TEAM_TEAMS_TIED;
const announcerSound_t AS_MATCH_OVERTIME		= AS_GENERAL_SUDDEN_DEATH;
const announcerSound_t AS_MATCH_LAST_STANDING	= AS_GENERAL_ONE_FRAG;
const announcerSound_t AS_OBJECTIVE_ATTACKED	= AS_CTF_ENEMY_HAS_FLAG;
const announcerSound_t AS_OBJECTIVE_SECURED		= AS_CTF_YOUR_FLAG_RETURNED;

// Localized team name and colour escape, used by every message that names a
// team.  Quake 4's teams are Marine and Strogg, which stand in for Quake
// Live's red and blue throughout the port.
const char *	MPLocalizedTeamName( int team );
const char *	MPTeamColor( int team );
// openQ4 END

const int VOTEMAPS_WAITING_MAPLIST		= (1<<0);
const int VOTEMAPS_WAITING_SAMAPLIST	= (1<<1);
const int VOTEMAPS_WAITING_LISTMAPS		= (1<<2);

typedef struct mpPlayerState_s {
	int				ping;			// player ping
	int				fragCount;		// kills
	int				teamFragCount;	// teamplay awards
	int				deadZoneScore;  // Score in dead zone
	int				wins;
	playerVote_t	vote;			// player's vote
	bool			scoreBoardUp;	// toggle based on player scoreboard button, used to activate de-activate the scoreboard gui
	bool			ingame;
} mpPlayerState_t;

const int MAX_INSTANCES = 8;

const int NUM_CHAT_NOTIFY	= 5;
const int CHAT_FADE_TIME	= 400;
const int FRAGLIMIT_DELAY	= 2000;
const int CAPTURELIMIT_DELAY = 750;

const int MP_PLAYER_MINFRAGS = -100;
const int MP_PLAYER_MAXFRAGS = 999;
const int MP_PLAYER_MAXWINS	= 100;
const int MP_PLAYER_MAXPING	= 999;

const int MP_PLAYER_MAXKILLS = 999;
const int MP_PLAYER_MAXDEATHS = 999;

// openQ4: team scores ride the snapshot as a short.  Domination accrues a
// point per control point per tick, so an unclamped total could wrap.
const int MP_TEAM_MAXSCORE = 30000;

const int MAX_AP = 5;

const int CHAT_HISTORY_SIZE = 2048;
const int RCON_HISTORY_SIZE = 4096;

const int KILL_NOTIFICATION_LEN = 256;
//RAVEN BEGIN
//asalmon: update stats for Xenon
#ifdef _XENON
const int XENON_STAT_UPDATE_INTERVAL = 1000;
#endif

const int ASYNC_PLAYER_FRAG_BITS = -idMath::BitsForInteger( MP_PLAYER_MAXFRAGS - MP_PLAYER_MINFRAGS );	// player can have negative frags
const int ASYNC_PLAYER_WINS_BITS = idMath::BitsForInteger( MP_PLAYER_MAXWINS );
const int ASYNC_PLAYER_PING_BITS = idMath::BitsForInteger( MP_PLAYER_MAXPING );
const int ASYNC_PLAYER_INSTANCE_BITS = idMath::BitsForInteger( MAX_INSTANCES );
const int ASYNC_PLAYER_DEATH_BITS = idMath::BitsForInteger( MP_PLAYER_MAXDEATHS );
const int ASYNC_PLAYER_KILL_BITS = idMath::BitsForInteger( MP_PLAYER_MAXKILLS );
//RAVEN END
//RITUAL BEGIN
const int MAX_TEAM_POWERUPS = 5;
//RITUAL END
// ddynerman: game state
#include "mp/GameState.h"
// openQ4: gametypes carried over from Quake Live
#include "mp/RoundGameState.h"
#include "mp/RoundModes.h"
#include "mp/Duel.h"

typedef struct mpChatLine_s {
	idStr			line;
	short			fade;			// starts high and decreases, line is removed once reached 0
} mpChatLine_t;

typedef struct mpBanInfo_s {
	idStr			name;
	char			guid[ CLIENT_GUID_LENGTH ];
//	unsigned char	ip[ 15 ];
} mpBanInfo_t;

class idPhysics_Player;

class idMultiplayerGame {

	// rvGameState manages our state
	friend class rvGameState;

public:

					idMultiplayerGame();

	void			Shutdown( void );

	// resets everything and prepares for a match
	void			Reset( void );

// RAVEN BEGIN
// mwhitlock: Dynamic memory consolidation
	// Made this public so that level heap can be emptied.
	void			Clear( void );
// RAVEN END

	// setup local data for a new player
	void			SpawnPlayer( int clientNum );

	// Run the MP Game
	void			Run( void );

	// Run the local client
	void			ClientRun( void );
	void			ClientEndFrame( void );

	// Run common code (client & server)
	void			CommonRun( void );

	// draws mp hud, scoredboard, etc.. 
	bool			Draw( int clientNum );
	

	// updates a player vote
	void			PlayerVote( int clientNum, playerVote_t vote );

	// updates frag counts and potentially ends the match in sudden death
	void			PlayerDeath( idPlayer *dead, idPlayer *killer, int methodOfDeath );

	void			AddChatLine( const char *fmt, ... ) id_attribute((format(printf,2,3)));
	void			PrintChatLine( const char *message, const bool teamChat );

// RITUAL BEGIN
// squirrel: Mode-agnostic buymenus
	void			OnBuyModeTeamVictory( int winningTeam );
// squirrel: added DeadZone multiplayer mode
	void			OnDeadZoneTeamVictory( int winningTeam );
// RITUAL END

	void			UpdateMainGui( void );
// RAVEN BEGIN
// bdube: global pickup sounds (powerups, etc)
	// Global item acquire sounds 
	void			PlayGlobalItemAcquireSound ( int entityDefIndex );

	bool			CanTalk( idPlayer *from, idPlayer *to, bool echo );
	void			ReceiveAndForwardVoiceData( int clientNum, const idBitMsg &inMsg, int messageType );

#ifdef _USE_VOICECHAT
// jscott: game side voice comms
	void			XmitVoiceData( void );
	void			ReceiveAndPlayVoiceData( const idBitMsg &inMsg );
#endif

// jshepard: selects a map at random that will run with the current game type
	bool			PickMap( idStr gameType, bool checkOnly = false );
	void			ClearVote( int clientNum = -1 );
	void			ResetRconGuiStatus( void );

// RAVEN END

	idUserInterface*	StartMenu( void );
	void			ShowInitialJoinMenu( void );

	const char*		HandleGuiCommands( const char *menuCommand );

	void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	void			ReadFromSnapshot( const idBitMsgDelta &msg );

	void			ShuffleTeams( void );
	void			SetGameType( void );
	void			SetMatchStartedTime( int time ) { matchStartedTime = time; }

	rvGameState*	GetGameState( void );

	// One authoritative competitive aggregate owns phase, round, clocks and
	// frozen rules.  rvGameState remains the gameplay adapter, but may mutate
	// its legacy state only after these methods accept the same transition.
	const mpMatchSession &GetMatchSession( void ) const { return matchSession; }
	const mpCompetitiveRules &GetCompetitiveRules( void ) const { return matchRules; }
	const mpCompetitionSeries &GetCompetitionSeries( void ) const { return matchSeries; }
	const mpSessionView *GetClientMatchView( void ) const {
		return clientMatchViewValid ? &clientMatchView : NULL;
	}
	// Resolve both current slot-generation bindings on every attempt.  This is
	// the authoritative camera gate; recipient views are discovery data only.
	bool			CanSpectatorFollow( int observerSlot, int targetSlot ) const;
	// Placed, respawning major items report only authoritative lifecycle
	// transitions.  The registry derives identity and recipient disclosure;
	// item code never writes client UI or network state directly.
	void			ObserveCompetitiveItemPickup( const idItem *item,
						float respawnSeconds );
	void			ObserveCompetitiveItemAvailable( const idItem *item );
	// Called once at the authoritative frame boundary, before any gameplay
	// input, events or entity thinking.  A pending pause therefore takes effect
	// atomically for the whole simulation frame.
	void			BeginCompetitiveFrame( void );
	bool			IsGameplayFrozen( void ) const;
	bool			CanCommitMatchPhaseTransition( mpGameState_t newState ) const;
	bool			CommitMatchPhaseTransition( mpGameState_t newState );
	bool			CommitMatchPhaseTransition( mpGameState_t newState,
						mpMatchTransitionReason_t reason, mpParticipantId authorizer );
	bool			CommitMatchPhaseTransition( mpGameState_t newState,
						mpMatchTransitionReason_t reason, mpParticipantId authorizer,
						int forfeitingSide );
	bool			CommitMatchRoundTransition( roundState_t newState );
	bool			BeginMatchOvertimePeriod( void );
	void			ServerReceiveMatchOperation( int clientNum, const idBitMsg &msg );
	void			ClientReceiveMatchOperationResult( const idBitMsg &msg );
	void			ClientReceiveMatchView( const idBitMsg &msg );
	void			ClientReceiveRefereeAuthChallenge( const idBitMsg &msg );
	bool			SubmitMatchOperation( mpMatchOperationRequest_t &request );
	bool			RequestRefereeAuthentication( const char *password );


	void			PrintMessageEvent( int to, msg_evt_t evt, int parm1 = -1, int parm2 = -1 );
	void			PrintMessage( int to, const char* message );

// openQ4 BEGIN
// Server driven centre-screen notice.  Quake 4 had no such channel: every big
// message was derived client side from an observed state delta, which cannot
// express text that varies with who did what to whom.  The payload is a
// localization id plus up to two typed parameters, so nothing on the wire is
// pre-translated.
	typedef enum {
		CPARM_NONE = 0,		// slot unused
		CPARM_INT,			// printed as a number
		CPARM_CLIENT,		// resolved to that client's name, with team colour
		CPARM_TEAM			// resolved to the localized team name
	} centerPrintParm_t;

	// to == -1 broadcasts.  strId must be a #str_ id, never display text.
	void			CenterPrint( int to, const char *strId, bool persist = false );
	void			CenterPrint( int to, const char *strId, centerPrintParm_t type1, int parm1, bool persist = false );
	void			CenterPrint( int to, const char *strId, centerPrintParm_t type1, int parm1, centerPrintParm_t type2, int parm2, bool persist = false );
	void			CenterPrintTeam( int team, const char *strId, centerPrintParm_t type1 = CPARM_NONE, int parm1 = 0, bool persist = false );
	void			CenterPrintTeam( int team, const char *strId, centerPrintParm_t type1, int parm1, centerPrintParm_t type2, int parm2, bool persist = false );
	void			ReceiveCenterPrint( const idBitMsg &msg );

	// Server driven announcer cue.  ScheduleAnnouncerSound only ever reaches the
	// machine it runs on, so a cue decided by server-only game logic - who was
	// left standing, whose round it was - never reached the player it was about.
	// to == -1 broadcasts.
	void			AnnounceTo( int to, announcerSound_t sound );
	void			AnnounceToTeam( int team, announcerSound_t sound );
	void			ReceiveAnnouncer( const idBitMsg &msg );

	// authoritative ready state, set from the client's reliable ready message
	void			ServerSetPlayerReady( int clientNum, bool ready );
	bool			IsManagedMatch( void ) const;
	// Legacy server administration is intentionally unavailable while the
	// revisioned Match Control authority owns the session.  Command and GUI
	// adapters call this before touching cvars, maps, teams, or clients.
	bool			RejectManagedLegacyMutation( const char *action );
	bool			ServerReconcileManagedUserInfo( int clientNum, idDict &userInfo );
	static void		Ready_f( const idCmdArgs &args );
	static void		NotReady_f( const idCmdArgs &args );
	static void		ReadyUp_f( const idCmdArgs &args );
// openQ4 END

	void			DisconnectClient( int clientNum );
	static void		ForceReady_f( const idCmdArgs &args );
	static void		SeriesBind_f( const idCmdArgs &args );
	static void		Broadcaster_f( const idCmdArgs &args );
	static void		DropWeapon_f( const idCmdArgs &args );
	static void		MessageMode_f( const idCmdArgs &args );
	static void		VoiceChat_f( const idCmdArgs &args );
	static void		VoiceChatTeam_f( const idCmdArgs &args );


// RAVEN BEGIN
// shouchard:  added console commands to mute/unmute voice chat
	static void		VoiceMute_f( const idCmdArgs &args );
	static void		VoiceUnmute_f( const idCmdArgs &args );

// jshepard: command wrappers
	static void		ForceTeamChange_f( const idCmdArgs& args );
	static void		RemoveClientFromBanList_f( const idCmdArgs& args );
	
// autobalance helper for the guis
	static void		CheckTeamBalance_f( const idCmdArgs &args );

// activates the admin console when a rcon password challenge returns.
	void			ProcessRconReturn( bool success );
// RAVEN END

	typedef enum {
		VOTE_RESTART = 0,
		VOTE_TIMELIMIT,
		VOTE_FRAGLIMIT,
		VOTE_GAMETYPE,
		VOTE_KICK,
		VOTE_MAP,
		VOTE_BUYING,
		VOTE_NEXTMAP,
// RAVEN BEGIN
// shouchard:  added capturelimit, round limit, and autobalance to vote flags
		VOTE_CAPTURELIMIT,
		VOTE_ROUNDLIMIT,
		VOTE_AUTOBALANCE,
		VOTE_MULTIFIELD,	// all the "packed" vote functions
// RAVEN END
		VOTE_CONTROLTIME,
		VOTE_COUNT,
		VOTE_NONE
	} vote_flags_t;

	typedef enum {
		VOTE_UPDATE,
		VOTE_FAILED,
		VOTE_PASSED,	// passed, but no reset yet
		VOTE_ABORTED,
		VOTE_RESET		// tell clients to reset vote state
	} vote_result_t;

// RAVEN BEGIN
// shouchard:  added enum to remove magic numbers
	// openQ4: mirrors mpVoteGameTypeOrder[] in mp/GameTypes.cpp, which is the
	// authority.  The gametype menu dropdowns index this positionally, so it
	// is append-only.
	typedef enum {
		VOTE_GAMETYPE_DM = 0,
		VOTE_GAMETYPE_TOURNEY,
		VOTE_GAMETYPE_TDM,
		VOTE_GAMETYPE_CTF,
		VOTE_GAMETYPE_ARENA_CTF,
//RITUAL BEGIN
//
		VOTE_GAMETYPE_DEADZONE,
//RITUAL END
		VOTE_GAMETYPE_DUEL,
		VOTE_GAMETYPE_CA,
		VOTE_GAMETYPE_FREEZETAG,
		VOTE_GAMETYPE_REDROVER,
		VOTE_GAMETYPE_1F_CTF,
		VOTE_GAMETYPE_ARENA_1F_CTF,
		VOTE_GAMETYPE_OVERLOAD,
		VOTE_GAMETYPE_HARVESTER,
		VOTE_GAMETYPE_DOMINATION,
		VOTE_GAMETYPE_ATTACK_DEFEND,
		VOTE_GAMETYPE_COUNT
	} vote_gametype_t;
// RAVEN END

	void			SendMapList( int clientNum );
	void			ReadMapList( const idBitMsg &msg );

	static void		Vote_f( const idCmdArgs &args );
	static void		CallVote_f( const idCmdArgs &args );
	void			ClientCallVote( vote_flags_t voteIndex, const char *voteValue );
	void			ServerCallVote( int clientNum, const idBitMsg &msg );
	void			ClientStartVote( int clientNum, const char *voteString );
	void			ServerStartVote( int clientNum, vote_flags_t voteIndex, const char *voteValue );
// RAVEN BEGIN
// shouchard:  multiline vote support
	void			ClientUpdateVote( vote_result_t result, int yesCount, int noCount, const voteStruct_t &voteData );
// RAVEN END
	void			CastVote( int clientNum, bool vote );
	void			ExecuteVote( void );
// RAVEN BEGIN
// shouchard:  multiline vote handlers
	void			ClientCallPackedVote( const voteStruct_t &voteData );
	void			ServerCallPackedVote( int clientNum, const idBitMsg &msg );
	void			ClientStartPackedVote( int clientNum, const voteStruct_t &voteData );
	void			ServerStartPackedVote( int clientNum, const voteStruct_t &voteData );
	void			ExecutePackedVote( void );
	const char *	LocalizeGametype( void );
// RAVEN END

	void			WantKilled( int clientNum );
	int				NumActualClients( bool countSpectators, int *teamcount = NULL );
	void			DropWeapon( int clientNum );
	void			MapRestart( void );
	void			JoinTeam( const char* team );
	// called by idPlayer whenever it detects a team change (init or switch)
	void			SwitchToTeam( int clientNum, int oldteam, int newteam );
	
	bool			IsPureReady( void ) const;
	void			ProcessChatMessage( int clientNum, bool team, const char *name, const char *text,
									const char *sound, bool triggerBotReplies );
	void			ProcessVoiceChat( int clientNum, bool team, int index );
// RAVEN BEGIN
// shouchard:  added commands to mute/unmute voice chat
	void			ClientVoiceMute( int clientNum, bool mute );
	int				GetClientNumFromPlayerName( const char *playerName );
	void			ServerHandleVoiceMuting( int clientSrc, int clientDest, bool mute );
// shouchard:  fixing a bug in multiplayer where round timer sounds (5 minute
//             warning, etc.) don't go away at the end of the round.
	void			ClearAnnouncerSounds( void );
// shouchard:  server admin stuff
	typedef struct 
	{
		bool		restartMap;
		idStr		mapName;
		int			gameType;
		int			captureLimit;
		int			fragLimit;
		int			tourneyLimit;
		int			timeLimit;
		int			controlTime;
		bool		buying;
		bool		autoBalance;
		bool		shuffleTeams;
	} serverAdminData_t;

	void			HandleServerAdminBanPlayer( int clientNum );
	void			HandleServerAdminRemoveBan( const char * info );
	void			HandleServerAdminKickPlayer( int clientNum );
	void			HandleServerAdminForceTeamSwitch( int clientNum );
	bool			HandleServerAdminCommands( serverAdminData_t &data );
// RAVEN END

// RITUAL BEGIN
	typedef struct mpTeamPowerups_s {
		int powerup;
		int time;
		bool update;
		int endTime;
	} mpTeamPowerups_t;

	mpTeamPowerups_t teamPowerups[TEAM_MAX][MAX_TEAM_POWERUPS];

	void			AddTeamPowerup(int powerup, int time, int team);
	void			UpdateTeamPowerups();
	void			SetUpdateForTeamPowerups(int team);
// RITUAL END

	void			Precache( void );
	
	// throttle UI switch rates
	void			ThrottleUserInfo( void );
	void			ToggleSpectate( void );
	void			ToggleReady( void );
	void			ToggleTeam( void );

	void			ClearFrags( int clientNum );

	void			EnterGame( int clientNum );
	bool			CanPlay( idPlayer *p );
	bool			IsInGame( int clientNum );
	bool			WantRespawn( idPlayer *p );

	void			ServerWriteInitialReliableMessages( const idMessageSender &sender, int clientNum );
	void			ClientReadStartState( const idBitMsg &msg );

	void			ServerClientConnect( int clientNum );

	void			PlayerStats( int clientNum, char *data, const int len );

	void			AddTeamScore ( int team, int amount );
	void			AddPlayerScore( idPlayer* player, int amount );
	void			AddPlayerTeamScore( idPlayer* player, int amount );
	void			AddPlayerWin( idPlayer* player, int amount );
	void			SetPlayerTeamScore( idPlayer* player, int value );
	void			SetPlayerDeadZoneScore( idPlayer* player, float value );
	void			SetPlayerScore( idPlayer* player, int value );
	void			SetPlayerWin( idPlayer* player, int value );
	void			SetHudOverlay( idUserInterface* overlay, int duration );

	void			ClearMap ( void );

	void			EnableDamage( bool enable = true );

	idPlayer*		GetRankedPlayer( int i );
	int				GetRankedPlayerScore( int i );
	int				GetNumRankedPlayers( void );
	
	idPlayer*		GetUnrankedPlayer( int i );
	int				GetNumUnrankedPlayers( void );

	int				GetScore( int i );
	int				GetScore( idPlayer* player );

	int				GetTeamScore( int i );
	int				GetTeamScore( idPlayer* player );

	int				GetWins( int i );
	int				GetWins( idPlayer* player );

	// asalmon: Get the score for a team.
	int				GetScoreForTeam( int i );
	int				GetTeamsTotalFrags( int i );
	int				GetTeamsTotalScore( int i );
	idUserInterface *GetMainGUI() {return mainGui;}

	float			GetPlayerDeadZoneScore(idPlayer* player);

	int				TeamLeader( void );

	int				GetPlayerTime( idPlayer* player );

	const char*		GetLongGametypeName( const char* gametype );
	const char*		VoteGameTypeToString( int gameTypeInt );
	int				GameTypeToVote( const char *gameType );

	void			ReceiveRemoteConsoleOutput( const char* output );

	void			ClientSetInstance( const idBitMsg& msg );
	void			ServerSetInstance( int instance );

	void			AddPrivatePlayer( int clientId );
	void			RemovePrivatePlayer( int clientId );

//RAVEN BEGIN
//asalmon: Xenon scoreboard update
#ifdef _XENON
	void			UpdateXenonScoreboard( idUserInterface *scoreBoard );
	int				lastScoreUpdate;
// mekberg: for selecting local player
	void			SelectLocalPlayer( idUserInterface *scoreBoard );
#endif
//RAVEN END
	int				VerifyTeamSwitch( int wantTeam, idPlayer *player );

	void			RemoveAnnouncerSound( int type );
	void			RemoveAnnouncerSoundRange( int startType, int endType );
	void			ScheduleAnnouncerSound ( announcerSound_t sound, float time, int instance = -1, bool allowOverride = false );
	void			ScheduleTimeAnnouncements( void );
// RAVEN END

	void			SendDeathMessage( idPlayer *attacker, idPlayer *victim, int methodOfDeath, bool quadKill );
	void			ReceiveDeathMessage( idPlayer *attacker, int attackerScore, idPlayer *victim, int victimScore, int methodOfDeath, bool quadKill );

	rvCTF_AssaultPoint*		NextAP( int team );
	int						OpposingTeam( int team );

	idList<idEntityPtr<rvCTF_AssaultPoint> > assaultPoints;

	// Buying Manager - authority for buying system game balance constants (awards,
	// costs, etc.)
	riBuyingManager	mpBuyingManager;

	idUserInterface* statSummary;			// stat summary
	rvTourneyGUI	tourneyGUI;

	void			ShowStatSummary( void );
	// Called by rvGameState at the authoritative GAMEON/GAMEREVIEW
	// transitions.  Keeping campaign result capture on the transition itself
	// means disconnect/forfeit paths cannot skip it by changing state outside
	// idMultiplayerGame::Run.
	void			OnMatchStarted( void );
	void			OnMatchEnded( void );
	// The Arena campaign is a local single-player experience hosted by the MP
	// game.  Keep its staging and review presentation behind the campaign token
	// so ordinary multiplayer retains the stock ready, spectate and camera flow.
	bool			IsArenaCampaignMatch( void ) const;
	bool			ArenaCampaignLocksPlayers( void ) const;
	bool			ArenaCampaignAllowsFreeLook( void ) const;
	void			AdvanceArenaCampaignCeremony( void );
	void			SetupArenaCampaignStatSummary( void );
	int				GetMapWeaponMask( void );
	bool			ArenaCampaignFreezesWorld( void ) const;
	bool			BuildArenaCampaignSpawnInView( idPlayer *viewer, renderView_t *view );
	bool			BuildArenaCampaignIntroView( idPlayer *viewer, renderView_t *view );
	idPlayer *		ArenaCampaignIntroSubject( void );
	int				ArenaCampaignIntroCount( void );
	void			ShowArenaCampaignIntroCard( idPlayer *subject );
	bool			ArenaCampaignIntroBlocksCountdown( void );
	float			ArenaCampaignCeremonyFade( void ) const;
	void			DrawArenaCampaignCeremonyFade( void );
	bool			BuildArenaCampaignPresentationView( idPlayer *viewer, renderView_t *view );
	void			BeginArenaCampaignEntrancePresentation( void );
	void			ShowArenaCampaignVictoryPresentation( void );
	void			ClearArenaCampaignPresentation( void );
	bool			CanCapture( int team );
	void			FlagCaptured( idPlayer *player );
	
	void			UpdatePlayerRanks( playerRankMode_t rankMode = PRM_AUTO );
	void			UpdateTeamRanks( void );
	void			UpdateHud( idUserInterface* _mphud );
	idPlayer *		FragLimitHit( void );
	idPlayer *		FragLeader( void );
	bool			TimeLimitHit( void );
// openQ4 BEGIN
// Exit rules carried over from Quake Live.
	// true when the match is level at the top.  The optional output receives
	// the authoritative leading score, including valid zero/negative scores.
	bool			ScoreIsTied( int *leadingScore = NULL );
	// team that has pulled far enough ahead to end the match early, or -1
	int				MercyLimitHit( void );
	// team left holding the match when the other side empties out, or -1
	int				ForfeitTeam( void );
	// respawn delay in milliseconds while a match is in overtime, 0 otherwise
	int				GetOvertimeRespawnDelay( void );
	// total match length in milliseconds including every overtime granted
	int				GetMatchLengthMsec( void );
	// true while scoring is turned off, which is warmup unless si_warmupScoring
	bool			ScoringSuppressed( void ) const;
// openQ4 END
	int				GetCurrentMenu( void ) { return currentMenu; }

	void			SetFlagEntity( idEntity* ent, int team );
	idEntity*		GetFlagEntity( int team );

	void			WriteNetworkInfo( idFile *file, int clientNum );
	void			ReadNetworkInfo( idFile* file, int clientNum );

	void			SetShaderParms( renderView_t *view );
	
// RITUAL BEGIN
// squirrel: added DeadZone multiplayer mode
	int				NumberOfPlayersOnTeam( int team );
	int				NumberOfAlivePlayersOnTeam( int team );
	void			ReportZoneControllingPlayer( idPlayer* player );
	void			ReportZoneController(int team, int pCount, int situation, idEntity* zoneTrigger = 0);
	bool			IsValidTeam(int team);
	void			ControlZoneStateChanged( int team );

	void			ListMaps( void );

	int				powerupCount;
	int				prevAnnouncerSnd;
	int				defaultWinner;
	int				deadZonePowerupCount;
	dzState_t		dzState[ TEAM_MAX ];
	float			marineScoreBarPulseAmount;
	float			stroggScoreBarPulseAmount;
// RITUAL END

// RITUAL BEGIN
// squirrel: Mode-agnostic buymenus
	bool			isBuyingAllowedRightNow;

	void			OpenLocalBuyMenu( void );
	void			RedrawLocalBuyMenu( void );
	void			GiveCashToTeam( int team, float cashAmount );
	bool			IsBuyingAllowedInTheCurrentGameMode( void );
	bool			IsBuyingAllowedRightNow( void );
// RITUAL END
	static const char*	teamNames[ TEAM_MAX ];

private:
	static const char	*MPGuis[];
	static const char	*ThrottleVars[];
	static const char	*ThrottleVarsInEnglish[];
	static const int	ThrottleDelay[];

	char			killNotificationMsg[ KILL_NOTIFICATION_LEN ];

	int				pingUpdateTime;			// time to update ping

	mpPlayerState_t	playerState[ MAX_CLIENTS ];

	// game state
	rvGameState*	gameState;
	mpCompetitiveRules matchRules;
	mpMatchSession	matchSession;
	mpCompetitionSeries matchSeries;
	mpCompetitionSeriesReport matchSeriesReport;
	uint64_t		matchSeriesId;
	uint64_t		matchSeriesLinkedSessionId;
	mpSeriesRecoveryWorkspace matchSeriesRecoveryWorkspace;
	mpSeriesReportStorageWorkspace matchSeriesReportWorkspace;
	int			matchSeriesContestantSlot[ MP_SERIES_SIDE_COUNT ];
	uint64_t		matchSeriesContestantConnection[ MP_SERIES_SIDE_COUNT ];
	int			matchSeriesCompetitionSide[ MAX_CLIENTS ];
	uint64_t		matchSeriesCompetitionConnection[ MAX_CLIENTS ];
	int			matchSeriesGameSideForCompetition[ MP_SERIES_SIDE_COUNT ];
	bool			matchSeriesNeedsBindingRecovery;
	bool			matchSeriesAwaitingMapSession;
	bool			matchSessionOperational;
	uint64_t		nextMatchConnectionId;
	uint64_t		matchConnectionId[ MAX_CLIENTS ];
	mpMatchTeams	matchTeams;
	mpProposalService matchProposals;
	mpMatchOperationExecutor matchOperationExecutor;
	mpRefereeAuthenticationService matchRefereeAuthentication;
	mpMatchEvidence	matchEvidence;
	mpMatchEvidenceObserver matchEvidenceObserver;
	mpEvidenceStorageWorkspace matchEvidenceWorkspace;
	mpMatchItemTimingRegistry matchItemTiming;
	mpSessionView	clientMatchView;
	bool			clientMatchViewValid;
	mpMatchControlModel clientMatchControlModel;
	mpMatchControlError_t clientMatchControlError;
	bool			clientMatchControlErrorValid;
	uint64_t		clientMatchControlChoiceSessionId;
	uint64_t		clientMatchMenuProjectedViewRevision;
	uint64_t		clientMatchHudProjectedViewRevision;
	uint64_t		clientMatchScoreboardProjectedViewRevision;
	mpMatchOperationResult_t clientMatchOperationResult;
	bool			clientMatchOperationResultValid;
	mpMatchOperationRequest_t clientPendingMatchConfirmation;
	bool			clientPendingMatchConfirmationValid;
	uint64_t		matchViewRevision;
	uint64_t		matchControlRevision;
	uint64_t		matchViewObservedSessionRevision;
	uint32_t		matchViewObservedRulesRevision;
	uint64_t		matchViewObservedRulesDigest;
	mpProposalRevision_t matchViewObservedProposalRevision;
	uint64_t		matchViewObservedSeriesRevision;
	uint64_t		matchViewObservedTeamsRevision;
	uint64_t		matchViewObservedEvidenceRevision;
	uint64_t		matchViewObservedItemTimingRevision;
	bool			matchViewObservedEvidenceFinalized;
	bool			matchViewObservedEvidencePersisted;
	bool			matchViewObservedMVDRecording;
	bool			matchItemTimingNeedsInitialScan;
	int			matchViewNextClockUpdateTime;
	uint64_t		matchViewSentRevision[ MAX_CLIENTS ];
	uint32_t		lastMatchRequestId[ MAX_CLIENTS ];
	mpMatchOperationResult_t lastMatchRequestResult[ MAX_CLIENTS ];
	bool			lastMatchRequestResultValid[ MAX_CLIENTS ];
	int			matchOperationNextAllowedTime[ MAX_CLIENTS ][ MP_MATCH_COOLDOWN_COUNT ];
	uint32_t		nextClientMatchRequestId;
	uint32_t		nextTrustedLocalMatchRequestId;
	mpProposalId_t	nextMatchProposalId;
	uint64_t		nextMatchSessionId;
	bool			matchRefereeCredentialInitialized;
	bool			matchRefereeCredentialIsReal;
	char			pendingRefereePassword[ MP_REFEREE_AUTH_MAX_PASSWORD_BYTES + 1 ];
	int			pendingRefereePasswordLength;
	int			pendingRefereePasswordDeadline;
	mpRefereeAuthChallenge pendingRefereeChallenge;
	bool			pendingRefereeChallengeValid;
	bool			competitiveRulesValidForSession;
	bool			competitiveRulesInitialized;
	mpRuleValidationReason_t competitiveRulesFailure;
	bool			matchEvidenceFinalized;
	bool			matchEvidencePersisted;
	bool			matchEvidenceFinalizationPending;
	int				matchEvidenceMode;
	bool			matchMVDStartedBySession;
	bool			matchMVDAttemptedBySession;
	bool			matchMVDOperatorOwnedBySession;
	char			matchMVDQPath[ MP_MATCH_EVIDENCE_STORAGE_QPATH_BYTES + 1 ];
	uint64_t		matchPhaseEffectsSessionId;
	uint64_t		matchPhaseEffectsRevision;

	bool			InitializeCompetitiveRules( void );
	bool			CanEnterMatchCountdown( void ) const;
	bool			IsManagedTeamCommunicationActive( void ) const;
	bool			BuildManagedTeamCommunicationBinding( int clientNum,
						mpMatchTeamCommunicationBinding_t &binding ) const;
	mpMatchRulesValidationContext_t BuildCompetitiveRuleValidationContext( void ) const;
	bool			ConfigureMatchSessionFromCompetitiveRules( void );
	mpMatchTeamsPolicy_t BuildMatchTeamsPolicy( void ) const;
	bool			ApplyMatchTeamsTransaction(
						const mpMatchTeamsJoinDecision_t &decision,
						mpOperationExecutionResult_t &execution );
	bool			ApplyMatchSpectatorTransition( mpParticipantId participant,
						mpOperationExecutionResult_t &execution );
	void			ReconcileGameplayPhaseAfterMatchMutation( void );
	void			ApplyMatchTeamsPlanToLegacy(
						const mpMatchTeamsTransactionPlan_t &plan );
	void			ProcessMatchTeamQueue( void );
	bool			ConfigureMatchSessionForRules( mpMatchSession &session,
						const mpMatchRulesSnapshot &rules, bool rulesValid ) const;
	bool			BeginMatchSession( void );
	bool			BeginMatchEvidence( void );
	void			ReconcileMatchEvidenceForCommittedRules( void );
	bool			IsCompetitionSeriesModeSupported( void ) const;
	bool			CollectCompetitionSeriesContestants(
						int slots[ MP_SERIES_SIDE_COUNT ],
						uint64_t connections[ MP_SERIES_SIDE_COUNT ] ) const;
	int			ResolveCompetitionSide( mpParticipantId participant ) const;
	bool			BuildCompetitionSeriesMapPool(
						const mpSeriesProfileDescriptor &profile,
						char storage[ MP_SERIES_MAX_MAP_POOL ][ MP_SERIES_MAP_TOKEN_BYTES ],
						const char *tokens[ MP_SERIES_MAX_MAP_POOL ], int &count,
						mpSeriesReason_t &reason ) const;
	bool			PersistCompetitionSeries( void );
	bool			PersistCompetitionSeriesCandidate(
						const mpCompetitionSeries &series,
						const mpCompetitionSeriesReport &report,
						uint64_t seriesId, uint64_t linkedSessionId );
	bool			InitializeCompetitionSeriesReport(
						const mpCompetitionSeries &series, uint64_t seriesId,
						const int contestantSlots[ MP_SERIES_SIDE_COUNT ],
						mpCompetitionSeriesReport &report ) const;
	bool			FinalizeCompetitionSeriesReport(
						mpCompetitionSeries &series,
						mpCompetitionSeriesReport &report,
						mpParticipantId authorizer );
	bool			CommitCompetitionSeriesMapEvidence(
						const mpEvidenceStorageResult &evidenceStorage );
	bool			RestoreCompetitionSeriesIfRequested( void );
	bool			BindCompetitionSeriesContestant( int competitionSide,
						int clientNum );
	bool			ScheduleCompetitionSeriesMap( mpCompetitionSeries &candidate,
						const char *mapToken, mpOperationExecutionResult_t &execution );
	void			LinkCurrentSeriesEvidence( void );
	mpEvidenceCommittedStamp BuildMatchEvidenceStamp( void ) const;
	mpEvidenceActorRef MatchEvidenceActor( mpParticipantId participant,
						bool serverOperator = false ) const;
	void			ObserveMatchEvidence( mpParticipantId actor,
						bool serverOperator = false );
	void			RecordMatchEvidenceFinalStats( void );
	void			RecordMatchEvidenceParticipantStats( int clientNum,
						mpParticipantId participant );
	void			RecordMatchEvidenceResult( mpMatchTransitionReason_t reason,
						mpParticipantId authorizer, int forfeitingSide = MP_MATCH_SIDE_NONE );
	bool			PersistMatchEvidence(
						mpEvidenceStorageResult *storageResult = NULL );
	bool			FinalizeMatchEvidence( bool abortedIfUndecided );
	void			ProjectMatchMVDReportArtifact(
						mpSeriesReportArtifactInput &artifact ) const;
	bool			ReconcileCompetitionSeriesMVDResults(
						mpCompetitionSeriesReport &report, bool sealing );
	void			StartMatchMVDIfRequired( void );
	void			StopMatchMVD( const char *reason );
	bool			ApplyCommittedMatchPhaseEffects( int forfeitingSide );
	void			PublishCompetitiveRulesIdentity( void );
	void			MirrorCompetitiveRulesToLegacy( void );
	void			SynchronizeMatchParticipant( int clientNum );
	void			SynchronizeAllMatchParticipants( void );
	void			RebaseCompetitivePauseFrame( int deltaMsec );
	void			AdvanceMatchViewRevision( bool forceClockSample = false );
	void			InitializeMatchItemTimingObservations( void );
	bool			BuildMatchItemTimingIdentity( const idItem *item,
						mpMatchItemTimingKind_t &kind, char *adapterToken,
						int adapterTokenBytes ) const;
	void			ObserveCompetitiveItemState( const idItem *item,
						bool available, int respawnMsec );
	void			SendChangedMatchViews( bool force = false );
	mpMatchDisclosurePolicy_t BuildMatchDisclosurePolicy( void ) const;
	bool			BuildMatchDisclosureRecipient( int clientNum,
						mpParticipantId &participant,
						mpMatchDisclosureRecipient_t &recipient ) const;
	int				ResolveMatchDisclosureTargetSide(
						const mpMatchParticipantState &target ) const;
	bool			BuildMatchView( int clientNum, mpSessionView &view ) const;
	bool			AcceptClientMatchView( const mpSessionView &incoming );
	bool			RefreshLocalClientMatchView( void );
	void			ProjectClientMatchControlMenu( bool notifyGui );
	void			ProjectClientManagedMatchContext( idUserInterface *gui );
	bool			HandleMatchControlCommand( const char *token );
	void			ClearClientPendingMatchConfirmation( bool closeModal );
	void			ClearClientMatchControlConnectionState( bool clearGuiCredential );
	bool			WriteMatchViewMessage( int clientNum, idBitMsg &msg ) const;
	mpMatchViewAllowedOperationMask_t AllowedMatchOperationsFor( mpParticipantId participant ) const;
	mpMatchOperationResult_t MakeMatchOperationResult( const mpMatchOperationRequest_t &request,
						const mpOperationExecutionResult_t &execution ) const;
	bool			StoreClientMatchOperationResult(
						const mpMatchOperationResult_t &result );
	void			ClearMatchOperationTransportSlot( int clientNum );
	void			SendMatchOperationResult( int clientNum,
						const mpMatchOperationResult_t &result );
	bool			MatchOperationRateLimitAccepted( int clientNum,
						const mpMatchOperationDescriptor_t &descriptor );
	void			BuildMatchOperationContext( int clientNum,
						mpMatchOperationOpcode_t opcode, bool enforceTransportCooldown,
						mpOperationAdapterContext_t &context );
	void			ProcessPassedMatchProposals( void );
	bool			ApplyMatchOperationContinuation( int clientNum,
						const mpMatchOperationRequest_t &request,
						mpOperationExecutionResult_t &execution );
	bool			ExecuteTrustedLocalMatchOperation(
						mpMatchOperationRequest_t &request,
						mpOperationExecutionResult_t &execution );
	bool			InitializeRefereeAuthentication( void );
	bool			SendRefereeAuthChallenge( int clientNum,
						const mpRefereeAuthChallenge &challenge );
	bool			CompleteRefereeAuthChallenge( const mpRefereeAuthChallenge &challenge );
	void			ClearPendingRefereePassword( void );
	void			ApplyMatchOperationLegacyMirror( int clientNum,
						const mpMatchOperationRequest_t &request,
						const mpOperationExecutionResult_t &execution );
	mpMatchTransitionReason_t InferMatchTransitionReason( mpGameState_t from,
						mpGameState_t to ) const;
	mpMatchRoundTransitionReason_t InferRoundTransitionReason( roundState_t from,
						roundState_t to ) const;
	void			BeginArenaCampaignResult( void );
	void			UpdateArenaCampaignResult( void );

	// Ordered end-of-match ceremony. An Arena match is always one human on a
	// listen server with no remote clients, so these phases are host-local and
	// need no wire state - deliberately NOT mpGameState_t values, which are
	// replicated bytes every gametype branches on.
	enum arenaCeremonyPhase_t {
		ARENA_CEREMONY_NONE = 0,
		ARENA_CEREMONY_INTRO,		// warmup: present each authored opponent in turn
		ARENA_CEREMONY_SPAWN_IN,	// match start: orbit own body, land in first person
		ARENA_CEREMONY_TABLEAU,		// frozen world, player-steered orbit of the victor
		ARENA_CEREMONY_SCOREBOARD,
		ARENA_CEREMONY_STATS,
		ARENA_CEREMONY_DONE			// ceremony over; the result may be reported
	};
	int				arenaCeremonyPhase;
	int				arenaCeremonyPhaseStartTime;
	int				arenaCeremonyPhaseEndTime;
	int				arenaTableauStartTime;
	bool			arenaCeremonyFadeStarted;

	bool			arenaResultPending;
	bool			arenaResultReported;
	int				arenaResultToken;
	int				arenaResultOutcome;	// 0 loss, 1 win, 2 draw; framework handoff contract
	int				arenaResultPlayerScore;
	int				arenaResultOpponentScore;
	int				arenaResultReportTime;
	int				arenaPresentationVictor;
	int				arenaPresentationFocus;
	bool			arenaPresentationBlurEnabled;
	bool			arenaEntranceCameraResolved;
	// Which presentation the latched camera belongs to.  The entrance and the
	// final tableau both latch a collision-safe anchor, but they resolve it
	// around different points in the map, so the latch has to be re-resolved
	// when the phase changes instead of carrying the spawn shot into review.
	bool			arenaEntranceCameraIsEntrance;
	bool			arenaEntranceCameraFallback;
	bool			arenaEntranceCameraValid;
	idVec3			arenaEntranceCameraForward;
	idVec3			arenaEntranceCameraLeft;
	idVec3			arenaEntranceCameraRadial;
	float			arenaEntranceCameraHeightLimit;
	// Reference yaw for the final tableau's player-steered orbit, latched on its
	// first frame so the shot opens at the authored angle wherever the player
	// happened to be looking when the match ended.
	bool			arenaVictorLookLatched;
	float			arenaVictorLookYaw;
	// Match-start orbit basis, latched once so the shot cannot swim.
	// Weapon slots present as pickups on the loaded map, for the warmup
	// arsenal. Invalidated by ClearMap.
	int				mapWeaponMask;
	bool			mapWeaponMaskValid;
	bool			arenaSpawnInLatched;
	int				arenaIntroIndex;
	int				arenaIntroSubjectStartTime;
	int				arenaIntroArmDeadline;
	idVec3			arenaSpawnInForward;
	idVec3			arenaSpawnInLeft;

	void			SelectArenaCampaignPresentationFocus( idPlayer *host );
	idPlayer *		GetArenaCampaignPresentationFocus( void ) const;
	void			SetArenaCampaignDepthOfField( bool enabled, float focusDistance = 0.0f, float strength = 0.0f );

	// vote vars
	vote_flags_t	vote;					// active vote or VOTE_NONE
	int				voteTimeOut;			// when the current vote expires
	int				voteExecTime;			// delay between vote passed msg and execute
	int				yesVotes;				// counter for yes votes
	int				noVotes;				// and for no votes
	int				voteEligibleCount;		// frozen electorate size for the active inherited vote
	idStr			voteValue;				// the data voted upon ( server )
	idStr			voteString;				// the vote string ( client )
	bool			voted;					// hide vote box ( client )
	int				kickVoteMap[ MAX_CLIENTS ];
// RAVEN BEGIN
// shouchard:  names for kickVoteMap
	idStr			kickVoteMapNames[ MAX_CLIENTS ];
	voteStruct_t	currentVoteData;		// used for multi-field votes
// RAVEN END

	idStr			localisedGametype;

	idList<int>		voteMapDecls;
	int				voteMapsWaiting;

	// time related
	int				matchStartedTime;		// time current match started

	// guis
// RITUAL BEGIN
// squirrel: added DeadZone multiplayer mode
	//int				sqRoundNumber;			// round number in DeadZone; match expires when this equals "sq_numRoundsPerMatch" (cvar)
// squirrel: Mode-agnostic buymenus
	idUserInterface *buyMenu;				// buy menu
// RITUAL END
	idUserInterface *scoreBoard;			// scoreboard
	idUserInterface *mainGui;				// ready / nick / votes etc.
	idListGUI		*mapList;
	idUserInterface *msgmodeGui;			// message mode
	int				currentMenu;			// 0 - none, 1 - mainGui, 2 - msgmodeGui
	int				nextMenu;				// if 0, will do mainGui
	bool			bCurrentMenuMsg;		// send menu state updates to server


	enum {
		MPLIGHT_CTF_MARINE,
		MPLIGHT_CTF_STROGG,
		MPLIGHT_QUAD,
		MPLIGHT_HASTE,
		MPLIGHT_REGEN,
		MPLIGHT_MAX
	};

	int				lightHandles[ MPLIGHT_MAX ];
	renderLight_t	lights[ MPLIGHT_MAX ];

	// chat buffer
	idStr			chatHistory;

	// rcon buffer
	idStr			rconHistory;

//RAVEN BEGIN
//asalmon: Need to refresh stats periodically if the player is looking at stats
	int currentStatClient;
	int currentStatTeam;
//RAVEN END

public:
	// current player rankings
	idList<rvPair<idPlayer*, int> > 	rankedPlayers;
	idList<idPlayer*>		unrankedPlayers;

	rvPair<int, int>		rankedTeams[ TEAM_MAX ];

	// openQ4: live damage numbers for the local view.  Client side only, but
	// a listen server host stages into the same slab.
	rvDamageNumbers			damageNumbers;

private:

	int				lastVOAnnounce;

	int				lastReadyToggleTime;
	bool			pureReady;				// defaults to false, set to true once server game is running with pure checksums
	bool			currentSoundOverride;
	int				switchThrottle[ 3 ];
	int				voiceChatThrottle;

	void			SetupBuyMenuItems();

	idList<int>		privateClientIds;
	uint32_t		privatePlayers;

	// player who's rank info we're displaying
	idEntityPtr<idPlayer>		rankTextPlayer;

	idEntityPtr<idEntity>		flagEntities[ TEAM_MAX ];	
	idEntityPtr<idPlayer>		flagCarriers[ TEAM_MAX ];

	// updates the passed gui with current score information
	void			UpdateRankColor( idUserInterface *gui, const char *mask, int i, const idVec3 &vec );	

	// bdube: test scoreboard
	void			UpdateTestScoreboard( idUserInterface *scoreBoard );
	
	// ddynerman: gametype specific scoreboard
	void			UpdateScoreboard( idUserInterface *scoreBoard );

	void			UpdateDMScoreboard( idUserInterface *scoreBoard );
	void			UpdateTeamScoreboard( idUserInterface *scoreBoard );
	void			UpdateSummaryBoard( idUserInterface *scoreBoard );

	int				GetPlayerRank( idPlayer* player, bool& isTied );
	char*			GetPlayerRankText( idPlayer* player );
	char*			GetPlayerRankText( int rank, bool tied, int score );

	const char*		BuildSummaryListString( idPlayer* player, int rankedScore );

	void			UpdatePrivatePlayerCount( void );

	typedef struct announcerSoundNode_s {
		announcerSound_t					soundShader;
		float								time;
		idLinkList<announcerSoundNode_s>	announcerSoundNode;
		int									instance;
		bool								allowOverride;
	} announcerSoundNode_t;

	idLinkList<announcerSoundNode_t>	announcerSoundQueue;
	announcerSound_t					lastAnnouncerSound;

	static const char* announcerSoundDefs[ AS_NUM_SOUNDS ];

	float			announcerPlayTime;

	void			PlayAnnouncerSounds ( void );

	int				teamScore[ TEAM_MAX ];
	int				teamDeadZoneScore[ TEAM_MAX];
	void			ClearTeamScores ( void );

	// openQ4: last ready tally computed by AllPlayersReady, for the warmup HUD
	int				readyPlayerCount;
	int				eligiblePlayerCount;

	void			UpdateLeader( idPlayer* oldLeader );

	void			ClearGuis( void );
	void			DrawScoreBoard( idPlayer *player );
	void			CheckVote( void );
	bool			AbortInheritedVoteForManagedMatch( void );
	bool			AllPlayersReady( idStr* reason = NULL );
	
	const char *	GameTime( void );

	bool			EnoughClientsToPlay( void );
	void			DrawStatSummary( void );
	// go through the clients, and see if they want to be respawned, and if the game allows it
	// called during normal gameplay for death -> respawn cycles
	// and for a spectator who want back in the game (see param)
	void			CheckRespawns( idPlayer *spectator = NULL );
	// puts a spectating player's camera on a living team mate, if they have not
	// already picked one themselves
	void			FollowTeamMate( idPlayer *p );

	void			FreeLight ( int lightID );
	void			UpdateLight ( int lightID, idPlayer *player );
	void			CheckSpecialLights( void );
	void			ForceReady();
	// when clients disconnect or join spectate during game, check if we need to end the game
	void			CheckAbortGame( void );
	void			CheckAbortGame( mpParticipantId departedParticipant,
						int departedGameSide, int departedCompetitionSide );
	void			MessageMode( const idCmdArgs &args );
	void			DisableMenu( void );
	void			SetMapShot( void );
	// scores in TDM
	void			VoiceChat( const idCmdArgs &args, bool team );

// RAVEN BEGIN
// mekberg: added
	void			UpdateMPSettingsModel( idUserInterface* currentGui );
// RAVEN END

	void			WriteStartState( int clientNum, idBitMsg &msg, bool withLocalClient );

	bool			RequestVoteMaps( int flags );

	void			SetMapList( const char *listName, const char *mapName, int gameTypeInt );
	void			SetVoteMapList( void );
	void			SetSAMapList( void );
};

ID_INLINE bool idMultiplayerGame::IsPureReady( void ) const {
	return pureReady;
}

ID_INLINE void idMultiplayerGame::ClearFrags( int clientNum ) {
	playerState[ clientNum ].fragCount = 0;
}

ID_INLINE bool idMultiplayerGame::IsInGame( int clientNum ) {
	return playerState[ clientNum ].ingame;
}

ID_INLINE int idMultiplayerGame::OpposingTeam( int team ) {
	return (team == TEAM_STROGG ? TEAM_MARINE : TEAM_STROGG);
}

ID_INLINE idPlayer* idMultiplayerGame::GetRankedPlayer( int i ) {
	if( i >= 0 && i < rankedPlayers.Num() ) {
		return rankedPlayers[ i ].First();
	} else {
		return NULL;
	}
}

ID_INLINE int idMultiplayerGame::GetRankedPlayerScore( int i ) {
	if( i >= 0 && i < rankedPlayers.Num() ) {
		return rankedPlayers[ i ].Second();
	} else {
		return 0;
	}
}

ID_INLINE int idMultiplayerGame::GetNumUnrankedPlayers( void ) {
	return unrankedPlayers.Num();
}

ID_INLINE idPlayer* idMultiplayerGame::GetUnrankedPlayer( int i ) {
	if( i >= 0 && i < unrankedPlayers.Num() ) {
		return unrankedPlayers[ i ];
	} else {
		return NULL;
	}
}

ID_INLINE int idMultiplayerGame::GetNumRankedPlayers( void ) {
	return rankedPlayers.Num();
}

ID_INLINE int idMultiplayerGame::GetTeamScore( int i ) {
	return playerState[ i ].teamFragCount;
}

ID_INLINE int idMultiplayerGame::GetScore( int i ) {
	return playerState[ i ].fragCount;
}

ID_INLINE int idMultiplayerGame::GetWins( int i ) {
	return playerState[ i ].wins;
}

ID_INLINE void idMultiplayerGame::ResetRconGuiStatus( void ) {
	if( mainGui) {
		mainGui->SetStateInt( "password_valid", 0 );
	}
}

// asalmon: needed access team scores for rich presence
ID_INLINE int idMultiplayerGame::GetScoreForTeam( int i ) {
	if( i < 0 || i > TEAM_MAX ) {
		return 0;
	}
	return teamScore[ i ];
}

ID_INLINE int idMultiplayerGame::TeamLeader( void ) {
	if( teamScore[ TEAM_MARINE ] == teamScore[ TEAM_STROGG ] ) {
		return -1;
	} else {
		return ( teamScore[ TEAM_MARINE ] > teamScore[ TEAM_STROGG ] ? TEAM_MARINE : TEAM_STROGG );
	}
}

int ComparePlayersByScore( const void* left, const void* right );
int CompareTeamsByScore( const void* left, const void* right );

#endif	/* !__MULTIPLAYERGAME_H__ */

// RAVEN END
