#ifndef GUARD_MMO_H
#define GUARD_MMO_H

#include "global.h"

#define MMOLog(pBuf) DebugPrint("[MMO LOG]: MMO - " pBuf)
#define MMOLogf(pBuf, ...) DebugPrintf("[MMO LOG]: MMO - " pBuf, __VA_ARGS__)

#define COMMAND_SUCCESS         0xAAAA0710

enum {
    COMMAND_AUTH,
    COMMAND_AUTH_RESULT,
    COMMAND_AUTH_USERPASS,
    COMMAND_JOIN_MAP,
    COMMAND_JOIN_MAP_RESULT,
    COMMAND_MOVE,
    COMMAND_GAME_STATE,
    COMMAND_PLAYER_MOVEMENT,
};

enum AuthState {
    AUTH_STATE_NONE,
    AUTH_STATE_JWT_SENT,
    AUTH_STATE_NEEDS_LOGIN,
    AUTH_STATE_USERNAME_INPUT,
    AUTH_STATE_USERPASS_SENT,
    AUTH_STATE_AUTHENTICATED,
};

extern u8 gAuthState;

void ProcessCommand(u8 commandType, u8* commandParamsData, u16 commandParamsSize);
void SendCommand(u8 commandType, u8* commandParamsData, u16 commandParamsSize);
bool8 IsMMOObjectEvent(struct ObjectEvent *objectEvent);

#endif //GUARD_MMO_H