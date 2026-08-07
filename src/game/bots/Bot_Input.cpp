// Bot_Input.cpp
//




#include "../Game_local.h"

/*
==============
rvmBot::BotInputToUserCommand
==============
*/
// openQ4: how far ahead a bot checks for lava and slime, and the small lift used on every probe so
// it samples just inside the volume rather than exactly on the floor plane.
const float BOT_LIQUID_LOOKAHEAD	= 56.0f;
const float BOT_LIQUID_PROBE_LIFT	= 2.0f;

void rvmBot::BotInputToUserCommand(bot_input_t* bi, usercmd_t* ucmd, int time)
{
	idVec3 forward, right;

	//clear the whole structure
//	memset(ucmd, 0, sizeof(usercmd_t));
	//
	//common->Printf("dir = %f %f %f speed = %f\n", bi->dir[0], bi->dir[1], bi->dir[2], bi->speed);
	//the duration for the user command in milli seconds
	//
	if (bi->actionflags & ACTION_DELAYEDJUMP)
	{
		bi->actionflags |= ACTION_JUMP;
		bi->actionflags &= ~ACTION_DELAYEDJUMP;
	}
	//set the buttons
	if (bi->actionflags & ACTION_RESPAWN)
	{
		ucmd->buttons = BUTTON_ATTACK;
	}
	if (bi->actionflags & ACTION_ATTACK)
	{
		ucmd->buttons |= BUTTON_ATTACK;
	}
	//if (bi->actionflags & ACTION_TALK) ucmd->buttons |= BUTTON_TALK;
	//if (bi->actionflags & ACTION_GESTURE) ucmd->buttons |= BUTTON_GESTURE;
	//if (bi->actionflags & ACTION_USE) ucmd->buttons |= BUTTON_USE_HOLDABLE;
	if (bi->actionflags & ACTION_WALK)
	{
		ucmd->buttons |= BUTTON_RUN;
	}
	//if (bi->actionflags & ACTION_AFFIRMATIVE) ucmd->buttons |= BUTTON_AFFIRMATIVE;
	//if (bi->actionflags & ACTION_NEGATIVE) ucmd->buttons |= BUTTON_NEGATIVE;
	//if (bi->actionflags & ACTION_GETFLAG) ucmd->buttons |= BUTTON_GETFLAG;
	//if (bi->actionflags & ACTION_GUARDBASE) ucmd->buttons |= BUTTON_GUARDBASE;
	//if (bi->actionflags & ACTION_PATROL) ucmd->buttons |= BUTTON_PATROL;
	//if (bi->actionflags & ACTION_FOLLOWME) ucmd->buttons |= BUTTON_FOLLOWME;
	//
	ucmd->impulse |= bi->weapon;
	if (bi->lastWeaponNum != bi->weapon)
	{
		//ucmd->flags = UCF_IMPULSE_SEQUENCE;
		bi->lastWeaponNum = bi->weapon;
	}
	else
	{
		//ucmd->flags = 0;
	}

	idAngles botViewAngles = viewAngles;

	{
		int i;
		float move;
		float angMod = (1.0f / 12.0f);

		for (i = 0; i < 3; i++) {
			move = idMath::AngleDelta(bi->viewangles[i], botViewAngles[i]);
			botViewAngles[i] += (move * angMod);
		}
	}

	//set the view angles
	//NOTE: the ucmd->angles are the angles WITHOUT the delta angles
	ucmd->angles[0] = ANGLE2SHORT(botViewAngles[0] - deltaViewAngles[0]);
	ucmd->angles[1] = ANGLE2SHORT(botViewAngles[1] - deltaViewAngles[1]);
	ucmd->angles[2] = ANGLE2SHORT(botViewAngles[2] - deltaViewAngles[2]);

	bi->viewangles.ToVectors(&forward, &right, NULL);

	//bot input speed is in the range [0, 400]
	bi->speed = bi->speed * 127 / 400;
	//set the view independent movement
	ucmd->forwardmove = idMath::ClampChar(DotProduct(forward, bi->dir) * bi->speed);
	ucmd->rightmove = idMath::ClampChar(DotProduct(right, bi->dir) * bi->speed);
	//ucmd->upmove = abs(forward[2]) * bi->dir[2] * bi->speed;

	//normal keyboard movement
	if (bi->actionflags & ACTION_MOVEFORWARD)
	{
		ucmd->forwardmove += 127;
	}
	if (bi->actionflags & ACTION_MOVEBACK)
	{
		ucmd->forwardmove -= 127;
	}
	if (bi->actionflags & ACTION_MOVELEFT)
	{
		ucmd->rightmove -= 127;
	}
	if (bi->actionflags & ACTION_MOVERIGHT)
	{
		ucmd->rightmove += 127;
	}

	//jump/moveup
	//if (bi->actionflags & ACTION_JUMP)
	//	ucmd->buttons |= BUTTON_JUMP;

	//	ucmd->upmove += 127;
	//
	////crouch/movedown
	//if (bi->actionflags & ACTION_CROUCH)
	//	ucmd->upmove -= 127;
	//
	//Com_Printf("forward = %d right = %d up = %d\n", ucmd.forwardmove, ucmd.rightmove, ucmd.upmove);
	//Com_Printf("ucmd->serverTime = %d\n", ucmd->serverTime);

// openQ4 BEGIN
	// Liquid. The AAS files that ship with Quake 4 carry no swim reachabilities - Quake 4 has no
	// liquids for them to describe - so routing cannot help here, and this works off point queries
	// against the collision world instead. It only adds the vertical axis and vetoes suicidal
	// steps; the bot's own goal following still does the rest.
	{
		const idVec3 lift( 0.0f, 0.0f, BOT_LIQUID_PROBE_LIFT );
		const int hazardMask = CONTENTS_LAVA | CONTENTS_SLIME;
		const idVec3 origin = GetPhysics()->GetOrigin();
		const int feetLiquid = gameLocal.LiquidContentsAtPoint( origin + lift, this );

		if ( feetLiquid & hazardMask ) {
			// while it is burning, climbing out is the whole plan
			ucmd->upmove = 127;
			ucmd->buttons |= BUTTON_RUN;
		} else {
			if ( feetLiquid && gameLocal.LiquidContentsAtPoint( GetEyePosition(), this ) ) {
				// surface before the air runs out
				ucmd->upmove = 127;
			}

			if ( ucmd->forwardmove || ucmd->rightmove ) {
				idVec3 moveDir = forward * ucmd->forwardmove + right * ucmd->rightmove;
				moveDir.z = 0.0f;
				if ( moveDir.Normalize() > VECTOR_EPSILON ) {
					const idVec3 ahead = origin + moveDir * BOT_LIQUID_LOOKAHEAD + lift;
					if ( gameLocal.LiquidContentsAtPoint( ahead, this ) & hazardMask ) {
						ucmd->forwardmove = 0;
						ucmd->rightmove = 0;
					}
				}
			}
		}
	}
// openQ4 END

	if( bi->respawn )
	{
		ucmd->buttons |= BUTTON_ATTACK;
	}
}

/*
================
rvmBot::ResetUcmd
================
*/
void rvmBot::Bot_ResetUcmd( usercmd_t& ucmd )
{
	ucmd.forwardmove = 0;
	ucmd.rightmove = 0;
	//ucmd.upmove = 0;
	ucmd.impulse = 0;
	//ucmd.flags = 0;
	memset( &ucmd.buttons, 0, sizeof( ucmd.buttons ) );
}


/*
========================
rvmBot::BotInputFrame
========================
*/
void rvmBot::BotInputFrame( void )
{
	usercmd_t& ucmd = (usercmd_t &)gameLocal.usercmds[entityNumber];

	ucmd.gameTime = gameLocal.time;
	ucmd.gameFrame = gameLocal.framenum;
	ucmd.duplicateCount = 0;

	Bot_ResetUcmd(ucmd);
	BotInputToUserCommand( &bs.botinput, &ucmd, gameLocal.time );

	// pass the bot's user cmds off to the engine, so that other clients can predict this bot
	networkSystem->ServerSetBotUserCommand(entityNumber, gameLocal.framenum, ucmd);
}
