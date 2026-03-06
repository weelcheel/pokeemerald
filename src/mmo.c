#include "mmo.h"
#include "tcp.h"
#include "login_menu.h"
#include "constants/event_objects.h"
#include "event_object_movement.h"

#define MOVEMENT_BUFFER_SIZE 40

EWRAM_DATA u8 gGameUserId = 0;

static void ProcessAuthResult(u8* commandParamsData, u8 commandParamsSize);
static void ProcessJoinResult(u8* commandParamsData, u8 commandParamsSize);
static void ProcessGameState(u8* commandParamsData, u8 commandParamsSize);
static void ProcessPlayerMovement(u8* commandParamsData, u8 commandParamsSize);
static u8 ConvertMovementAction(u8 action);

static void ProcessAuthResult(u8* commandParamsData, u8 commandParamsSize)
{
    u32 result;
    if (commandParamsSize == 4)
    {
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

static void ProcessJoinResult(u8* commandParamsData, u8 commandParamsSize)
{
    if (commandParamsSize == 1)
    {
        gGameUserId = commandParamsData[0];
    }
}

// GameState is used only for spawning/positioning NPCs for players already on the map
static void ProcessGameState(u8* commandParamsData, u8 commandParamsSize)
{
    u8 playerCount = 0;
    u8 i, j;
    u8 gamePlayerId;

    s16 x, y;
    u8 currentElevation, facingDirection;
    u8 objectEventId;
    bool8 found;

    if (commandParamsSize >= 1)
    {
        playerCount = commandParamsData[0];
    }

    // Despawn MMO NPCs no longer in the game state
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (!gObjectEvents[i].active)
            continue;
        if (!IsMMOObjectEvent(&gObjectEvents[i]))
            continue;

        found = FALSE;
        for (j = 0; j < playerCount; j++)
        {
            gamePlayerId = commandParamsData[1 + (j * (8 + MOVEMENT_BUFFER_SIZE))];
            if (gamePlayerId == gGameUserId)
                continue;
            if (OBJ_EVENT_ID_MMO_FIRST + gamePlayerId == gObjectEvents[i].localId)
            {
                found = TRUE;
                break;
            }
        }
        if (!found)
        {
            RemoveMMOObjectEvent(i);
        }
    }

    // Spawn new NPCs or update existing ones
    for (i = 0; i < playerCount; i++)
    {
        gamePlayerId = commandParamsData[1 + (i * (8 + MOVEMENT_BUFFER_SIZE))];
        if (gamePlayerId == gGameUserId)
            continue;

        gamePlayerId = OBJ_EVENT_ID_MMO_FIRST + gamePlayerId;
        memcpy(&x, commandParamsData + 2 + (i * (8 + MOVEMENT_BUFFER_SIZE)), 2);
        memcpy(&y, commandParamsData + 4 + (i * (8 + MOVEMENT_BUFFER_SIZE)), 2);

        currentElevation = commandParamsData[7 + (i * (8 + MOVEMENT_BUFFER_SIZE))];
        facingDirection = commandParamsData[8 + (i * (8 + MOVEMENT_BUFFER_SIZE))];

        objectEventId = GetMMOObjectEventIdByLocalId(gamePlayerId);
        if (objectEventId == OBJECT_EVENTS_COUNT)
        {
            // Only spawn if we have a valid position (not 0,0 default)
            if (x != 0 || y != 0)
                SpawnSpecialObjectEventParameterized(OBJ_EVENT_GFX_BRENDAN_NORMAL, MOVEMENT_TYPE_NONE, gamePlayerId, x, y, currentElevation);
        }
        else
        {
            // Only correct position when NPC is not mid-movement to avoid teleporting
            if (!ObjectEventIsHeldMovementActive(&gObjectEvents[objectEventId]))
            {
                MoveObjectEventToMapCoords(&gObjectEvents[objectEventId], x, y);
                gObjectEvents[objectEventId].currentElevation = currentElevation;
            }
        }
    }
}

// Receives relayed movement actions from other players: [gameUserId(1), action(1)]
static void ProcessPlayerMovement(u8* commandParamsData, u8 commandParamsSize)
{
    u8 gamePlayerId;
    u8 action;
    u8 objectEventId;
    struct ObjectEvent* objectEvent;

    if (commandParamsSize < 2)
        return;

    gamePlayerId = commandParamsData[0];
    action = commandParamsData[1];

    if (gamePlayerId == gGameUserId)
        return;

    gamePlayerId = OBJ_EVENT_ID_MMO_FIRST + gamePlayerId;
    objectEventId = GetMMOObjectEventIdByLocalId(gamePlayerId);
    if (objectEventId == OBJECT_EVENTS_COUNT)
        return;

    objectEvent = &gObjectEvents[objectEventId];
    if (!objectEvent->active)
        return;

    action = ConvertMovementAction(action);

    // Face actions: apply immediately
    if (action <= MOVEMENT_ACTION_FACE_RIGHT)
    {
        u8 direction = action - MOVEMENT_ACTION_FACE_DOWN + 1; // FACE_DOWN=0 -> DIR_SOUTH=1
        ObjectEventTurnByLocalIdAndMap(objectEvent->localId, objectEvent->mapNum, objectEvent->mapGroup, direction);
        return;
    }

    // Walk actions: clear current movement and set new one
    ObjectEventClearHeldMovementIfActive(objectEvent);
    ObjectEventSetHeldMovement(objectEvent, action);
}

// Convert player-specific movement actions to NPC-compatible ones
static u8 ConvertMovementAction(u8 action)
{
    // Convert PLAYER_RUN to WALK_FAST (NPCs can't use player-specific actions)
    if (action >= MOVEMENT_ACTION_PLAYER_RUN_DOWN && action <= MOVEMENT_ACTION_PLAYER_RUN_RIGHT)
    {
        return MOVEMENT_ACTION_WALK_FAST_DOWN + (action - MOVEMENT_ACTION_PLAYER_RUN_DOWN);
    }
    return action;
}

void ProcessCommand(u8 commandType, u8* commandParamsData, u8 commandParamsSize)
{
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
        case COMMAND_PLAYER_MOVEMENT:
            ProcessPlayerMovement(commandParamsData, commandParamsSize);
            break;
        default:
            break;
    }
}

void SendCommand(u8 commandType, u8* commandParamsData, u8 commandParamsSize)
{
    u16 i;

    if (gOutgoingCommandsQueueSize + commandParamsSize + 2 > sizeof(gOutgoingCommandsQueue))
    {
        return;
    }
    gOutgoingCommandsQueue[gOutgoingCommandsQueueSize] = commandType;
    gOutgoingCommandsQueue[gOutgoingCommandsQueueSize + 1] = commandParamsSize;
    for (i = 0; i < commandParamsSize; i++)
    {
        gOutgoingCommandsQueue[gOutgoingCommandsQueueSize + 2 + i] = commandParamsData[i];
    }
    gOutgoingCommandsQueueSize += 2 + commandParamsSize;
    gOutgoingCommandsQueueCount++;
    gIsOutgoingCommandsQueueReady = TRUE;
}

bool8 IsMMOObjectEvent(struct ObjectEvent *objectEvent)
{
    return objectEvent->localId >= OBJ_EVENT_ID_MMO_FIRST
        && objectEvent->localId != OBJ_EVENT_ID_PLAYER;
}