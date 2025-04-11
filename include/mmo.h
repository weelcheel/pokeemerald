#ifndef GUARD_MMO_H
#define GUARD_MMO_H

#include "global.h"

#define MMOLog(pBuf) DebugPrint("[MMO LOG]: MMO - " pBuf)
#define MMOLogf(pBuf, ...) DebugPrintf("[MMO LOG]: MMO - " pBuf, __VA_ARGS__)

#define COMMAND_SUCCESS         0xAAAA0710

enum {
    COMMAND_AUTH,
    COMMAND_AUTH_RESULT,
    COMMAND_JOIN_MAP,
    COMMAND_JOIN_MAP_RESULT,
    COMMAND_MOVE,
    COMMAND_GAME_STATE,
};

void ProcessCommand(u8 commandType, u8* commandParamsData, u8 commandParamsSize);
void SendCommand(u8 commandType, u8* commandParamsData, u8 commandParamsSize);

#endif //GUARD_MMO_H