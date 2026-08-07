#include "../../idlib/precompiled.h"
#pragma hdrstop
#include "../Game_local.h"

// This equates to about 1 second
#define MAX_VOICE_PACKET_SIZE	382

idCVar si_voiceChat( "si_voiceChat", "1", CVAR_GAME | CVAR_BOOL | PC_CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_INFO, "enables or disables voice chat through the server" );
idCVar g_voiceChatDebug( "g_voiceChatDebug", "0", CVAR_GAME | CVAR_INTEGER | CVAR_NOCHEAT, "display info on voicechat net traffic" );

static bool ManagedVoiceTransportAllows( idPlayer *from, idPlayer *to,
		bool echo ) {
	if ( from == NULL || to == NULL || to->IsPlayerMuted( from ) ) {
		return false;
	}
	return from != to || echo;
}

void idMultiplayerGame::ReceiveAndForwardVoiceData( int clientNum, const idBitMsg &inMsg, int messageType ) {
	assert( clientNum >= 0 && clientNum < MAX_CLIENTS );

	idBitMsg	outMsg;
	int			i;
	byte		msgBuf[MAX_VOICE_PACKET_SIZE + 2];
	idPlayer *	from;
	const bool managedTeamCommunicationRequested = gameLocal.isServer &&
		matchRules.Committed().GetBool( MP_RULE_MANAGED_MATCH );
	bool managedTeamCommunication = false;
	mpMatchTeamCommunicationBinding_t managedSender;

	if ( clientNum < 0 || clientNum >= gameLocal.numClients ||
		clientNum >= MAX_CLIENTS ||
		!gameLocal.serverInfo.GetBool( "si_voiceChat" ) ) {
		return;
	}
	idEntity *fromEntity = gameLocal.entities[ clientNum ];
	if ( fromEntity == NULL ||
		!fromEntity->IsType( idPlayer::GetClassType() ) ) {
		return;
	}
	from = static_cast<idPlayer *>( fromEntity );

	if ( managedTeamCommunicationRequested ) {
		if ( !IsManagedTeamCommunicationActive() ||
			!BuildManagedTeamCommunicationBinding( clientNum, managedSender ) ||
			!MPMatchMayReceiveManagedTeamVoice( matchSession, managedSender,
				managedSender ) ) {
			return;
		}
		managedTeamCommunication = true;
	}

	const int payloadBytes = inMsg.GetRemainingData();
	if ( payloadBytes <= 0 || payloadBytes > MAX_VOICE_PACKET_SIZE ) {
		return;
	}

	// Create a new packet with forwarded data
	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.WriteByte( GAME_UNRELIABLE_MESSAGE_VOICEDATA_SERVER );
	outMsg.WriteByte( clientNum );
	outMsg.WriteData( inMsg.GetReadData(), payloadBytes );

	if( g_voiceChatDebug.GetInteger() & 2 ) {
		common->Printf( "VC: Received %d bytes, forwarding...\n", payloadBytes );
	}

	// Forward to appropriate parties
	for( i = 0; i < gameLocal.numClients; i++ )  {
		idEntity *toEntity = gameLocal.entities[ i ];
		if ( toEntity == NULL ||
			!toEntity->IsType( idPlayer::GetClassType() ) ) {
			continue;
		}
		idPlayer *to = static_cast<idPlayer *>( toEntity );
		if ( to->GetUserInfo() == NULL ||
			!to->GetUserInfo()->GetBool( "s_voiceChatReceive" ) ||
			i == gameLocal.localClientNum ) {
			continue;
		}

		bool mayReceive = false;
		if ( managedTeamCommunication ) {
			mpMatchTeamCommunicationBinding_t managedRecipient;
			mayReceive = BuildManagedTeamCommunicationBinding( i,
				managedRecipient ) &&
				MPMatchMayReceiveManagedTeamVoice( matchSession, managedSender,
					managedRecipient ) &&
				ManagedVoiceTransportAllows( from, to, !!( messageType & 1 ) );
		} else {
			mayReceive = CanTalk( from, to, !!( messageType & 1 ) );
		}
		if ( !mayReceive ) {
			continue;
		}

		if( messageType & 2 )
		{
			// If "from" is testing - then only send back to him
			if( from == to )
			{
				gameLocal.SendUnreliableMessage( outMsg, to->entityNumber );
			}
		}
		else
		{
			if( to->AllowedVoiceDest( from->entityNumber ) )
			{
				gameLocal.SendUnreliableMessage( outMsg, to->entityNumber );
				if( g_voiceChatDebug.GetInteger() & 2 )
				{
					common->Printf( " ... to client %d\n", to->entityNumber );
				}
			}
			else
			{
				if( g_voiceChatDebug.GetInteger() )
				{
					common->Printf( " ... suppressed packet to client %d\n", to->entityNumber );
				}
			}
		}
	}

#ifdef _USE_VOICECHAT
	// Listen servers need to manually call the receive function
	if ( gameLocal.isListenServer ) {
		idPlayer* to = gameLocal.GetLocalPlayer();
		if ( to != NULL && to->GetUserInfo() != NULL &&
			to->GetUserInfo()->GetBool( "s_voiceChatReceive" ) ) {
			bool mayReceive = false;
			if ( managedTeamCommunication ) {
				mpMatchTeamCommunicationBinding_t managedRecipient;
				mayReceive = BuildManagedTeamCommunicationBinding(
					gameLocal.localClientNum, managedRecipient ) &&
					MPMatchMayReceiveManagedTeamVoice( matchSession, managedSender,
						managedRecipient ) &&
					ManagedVoiceTransportAllows( from, to,
						!!( messageType & 1 ) );
			} else {
				mayReceive = CanTalk( from, to, !!( messageType & 1 ) );
			}
			if ( mayReceive ) {
				if( messageType & 2 )
				{
					// If "from" is testing - then only send back to him
					if( from == to )
					{
						// Skip over the unreliable-message control byte.
						outMsg.ReadByte();
						ReceiveAndPlayVoiceData( outMsg );
					}
				}
				else
				{
					if( to->AllowedVoiceDest( from->entityNumber ) )
					{
						if( g_voiceChatDebug.GetInteger() & 2 )
						{
							common->Printf( " ... to local client %d\n", gameLocal.localClientNum );
						}
						// Skip over the unreliable-message control byte.
						outMsg.ReadByte();
						ReceiveAndPlayVoiceData( outMsg );
					}
				}
			}
		}
	}
#endif
}

bool idMultiplayerGame::CanTalk( idPlayer *from, idPlayer *to, bool echo ) {
	if ( !to ) {
		return false;
	}

	if ( !from ) {
		return false;
	}

	if ( from->spectating != to->spectating ) {
		return false;
	}

	if ( to->IsPlayerMuted( from ) ) {
		return false;
	}

	if ( to->GetArena() != from->GetArena() ) {
		return false;
	}

	if ( gameLocal.IsTeamGame() && from->team != to->team ) {
		return false;
	}

	if ( from == to ) {
		return echo;
	}

	return true;
}

#ifdef _USE_VOICECHAT

void idMultiplayerGame::XmitVoiceData( void )
{
	idBitMsg	outMsg;
	int			count;
	byte		work;
	byte		buffer[MAX_VOICE_PACKET_SIZE];
	byte		msgBuf[MAX_VOICE_PACKET_SIZE + 1];

	if( !gameLocal.serverInfo.GetBool( "si_voiceChat" ) )
	{
		return;
	}

	outMsg.Init( msgBuf, sizeof( msgBuf ) );

	// Grab any new input and send up to the server
// jmarshall - voice capture is not available through the engine sound interface.
	//count = soundSystem->GetVoiceData( buffer, MAX_VOICE_PACKET_SIZE );
	count = 0;
// jmarshall end
	if( count )
	{
		outMsg.BeginWriting();
		outMsg.BeginReading();

		work = GAME_RELIABLE_MESSAGE_VOICEDATA_CLIENT + cvarSystem->GetCVarBool( "s_voiceChatEcho" ) + ( cvarSystem->GetCVarInteger( "s_voiceChatTest" ) * 2 );
		outMsg.WriteByte( work );
		outMsg.WriteData( buffer, count );

		// FIXME!!! FIXME!!! This should be unreliable - this is very bad
		networkSystem->ClientSendReliableMessage( outMsg );

		if( g_voiceChatDebug.GetInteger() & 1 )
		{
			common->Printf( "VC: sent %d bytes at %d\n", count, gameLocal.time );
		}
	}
}

void idMultiplayerGame::ReceiveAndPlayVoiceData( const idBitMsg &inMsg )
{
	int		clientNum;

	if( !gameLocal.serverInfo.GetBool( "si_voiceChat" ) )
	{
		return;
	}
	
	clientNum = inMsg.ReadByte();
// jmarshall - voice playback is not available through the engine sound interface; parse and discard.
	//soundSystem->PlayVoiceData( clientNum, inMsg.GetReadData(), inMsg.GetRemainingData() );
	(void)clientNum;
// jmarshall end
	if( g_voiceChatDebug.GetInteger() & 4 )
	{
		common->Printf( "VC: Playing %d bytes\n", inMsg.GetRemainingData() );
	}
}

#endif

// end
