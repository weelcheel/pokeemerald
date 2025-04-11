#include "mmo.h"
#include "tcp.h"
#include "login_menu.h"
#include "constants/event_objects.h"
#include "event_object_movement.h"

EWRAM_DATA u8 gMultiplayer[4096] = {0};
EWRAM_DATA u8 gGameUserId = 0;

void ProcessAuthResult(u8* commandParamsData, u8 commandParamsSize);
void ProcessJoinResult(u8* commandParamsData, u8 commandParamsSize);
void ProcessGameState(u8* commandParamsData, u8 commandParamsSize);

void ProcessAuthResult(u8* commandParamsData, u8 commandParamsSize)
{
    u32 result;
    if (commandParamsSize == 4)
    {
        // print each byte of the commandParamsData
        result = 0;
        result |= commandParamsData[0];
        result |= commandParamsData[1] << 8;
        result |= commandParamsData[2] << 16;
        result |= commandParamsData[3] << 24;
        MMOLogf("Auth result: %08X", result);

        if (result == COMMAND_SUCCESS)
        {
            MMOLog("Authentication successful!");
            sIsAuthenticated = TRUE;
            sHasSentAuthRequest = FALSE;
            Tcp_Authenticated();
        }
        else
        {
            MMOLog("Authentication failed!");
            sCancellationReason = TCP_CANCEL_CONNECTION_FAILED;
            sTcpState = TCP_STATE_DISCONNECTED;
        }
    }
}

void ProcessJoinResult(u8* commandParamsData, u8 commandParamsSize)
{
    if (commandParamsSize == 1)
    {
        gGameUserId = commandParamsData[0];
    }
}

void ProcessGameState(u8* commandParamsData, u8 commandParamsSize)
{
    u8 playerCount = 0;
    u8 i;
    u8 gamePlayerId;

    s16 x, y;
    u8 action, currentElevation, facingDirection;
    u8 objectEventId;

    if (commandParamsSize >= 1)
    {
        playerCount = commandParamsData[0];
    }

    for (i = 0; i < playerCount; i++)
    {
        gamePlayerId = commandParamsData[1 + i * 8];
        if (gamePlayerId == gGameUserId)
        {
            // do nothing for now, should do some things like corrections teleportations and so forth
        }
        else
        {
            gamePlayerId = OBJ_EVENT_ID_MMO_FIRST + gamePlayerId;
            x = 0;
            x = commandParamsData[2 + i * 8];
            x |= commandParamsData[3 + i * 8] << 8;

            y = 0;
            y = commandParamsData[4 + i * 8];
            y |= commandParamsData[5 + i * 8] << 8;

            action = commandParamsData[6 + i * 8];
            currentElevation = commandParamsData[7 + i * 8];
            facingDirection = commandParamsData[8 + i * 8];

            objectEventId = GetMMOObjectEventIdByLocalId(gamePlayerId);
            if (objectEventId == OBJECT_EVENTS_COUNT)
            {
                objectEventId = SpawnSpecialObjectEventParameterized(OBJ_EVENT_GFX_FAT_MAN, MOVEMENT_TYPE_NONE, gamePlayerId, x, y, currentElevation);
            }
            if (objectEventId != OBJECT_EVENTS_COUNT)
            {
                struct ObjectEvent* objectEvent = &gObjectEvents[objectEventId];
                MoveObjectEventToMapCoords(objectEvent, x, y);
            }
        }
    }
}

void ProcessCommand(u8 commandType, u8* commandParamsData, u8 commandParamsSize)
{
    MMOLogf("Processing command: %d", commandType);
    switch (commandType)
    {
        case COMMAND_AUTH_RESULT:
            ProcessAuthResult(commandParamsData, commandParamsSize);
            break;
        case COMMAND_JOIN_MAP_RESULT:
            ProcessJoinResult(commandParamsData, commandParamsSize);
            break;
        case COMMAND_GAME_STATE:
            ProcessGameState(commandParamsData, commandParamsSize);
            break;
        default:
            break;
    }
}

void SendCommand(u8 commandType, u8* commandParamsData, u8 commandParamsSize)
{
    u8 commandBytes[2 + commandParamsSize];
    u16 i;

    memset(commandBytes, 0, sizeof(commandBytes));
    if (gOutgoingCommandsQueueSize + commandParamsSize + 2 > sizeof(gOutgoingCommandsQueue))
    {
        // not enough space in the queue to send the command
        MMOLogf("Not enough space in outgoing commands queue for command %d", commandType);
        return;
    }
    commandBytes[0] = commandType;
    commandBytes[1] = commandParamsSize;
    for (i=0; i < commandParamsSize; i++)
    {
        commandBytes[2 + i] = commandParamsData[i];
    }
    // if (commandParamsSize > 0)
    // {
    //     memcpy(commandBytes + 2, commandParamsData, commandParamsSize);
    //     //memset(gOutgoingCommandsQueue + 2, commandParamsSize, commandParamsSize);
    // }
    // memcpy(gOutgoingCommandsQueue + gOutgoingCommandsQueueSize, commandBytes, commandParamsSize + 2);
    // gOutgoingCommandsQueueSize += commandParamsSize + 2;
    for (i=0; i < commandParamsSize + 2; i++)
    {
        gOutgoingCommandsQueue[gOutgoingCommandsQueueSize + i] = commandBytes[i];
    }
    gOutgoingCommandsQueueSize += 2+commandParamsSize;
    gOutgoingCommandsQueueCount++;
    gIsOutgoingCommandsQueueReady = TRUE;

    MMOLogf("Queued command %d with size %d | %d commands queued", commandType, commandParamsSize, gOutgoingCommandsQueueCount);
}