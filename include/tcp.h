#ifndef GUARD_TCP_H
#define GUARD_TCP_H

#include "global.h"

#define TCP_DATA_NOOP           0xFFFFFFFF
#define TCP_DATA_SUCCESS        0x0710
#define TCP_DATA_FAILURE        0x0711

#define TCP_HANDSHAKE           0xBAC7
#define TCP_HANDSHAKE_SUCCESS   TCP_HANDSHAKE | (TCP_DATA_SUCCESS << 16)
#define TCP_URL_META            0xBAC8
#define TCP_URL_META_SUCCESS    TCP_URL_META | (TCP_DATA_SUCCESS << 16)
#define TCP_URL_HEADER          0xBAC9
#define TCP_URL_SUCCESS         TCP_URL_HEADER | (TCP_DATA_SUCCESS << 16)
#define TCP_PORT_HEADER         0xBACA
#define TCP_PORT_SUCCESS        TCP_PORT_HEADER | (TCP_DATA_SUCCESS << 16)
#define TCP_CONNECT_HEADER      0xBACB
#define TCP_CONNECT_SUCCESS     TCP_CONNECT_HEADER | (TCP_DATA_SUCCESS << 16)
#define TCP_SEND_TO_SIO         0xBACC
#define TCP_RECEIVE_FROM_SIO    0xBACD

#define TCP_PACKET_MAGIC        0x07100420

#define TcpLog(pBuf) DebugPrint("[MMO LOG]: TCP - " pBuf)
#define TcpLogf(pBuf, ...) DebugPrintf("[MMO LOG]: TCP - " pBuf, __VA_ARGS__)

enum TcpState {
    TCP_STATE_INIT,
    TCP_STATE_HANDSHAKE,
    TCP_STATE_INIT_URL_META,
    TCP_STATE_INIT_URL_TRANSFER,
    TCP_STATE_PORT_TRANSFER,
    TCP_STATE_CONNECTING,
    TCP_STATE_CONNECTED,
    TCP_STATE_DISCONNECTED,
};

enum {
    TCP_CANCEL_TIMEOUT,
    TCP_CANCEL_CONNECTION_FAILED,
};

extern u8 gShouldAdvanceTcpState;

extern u16 gOutgoingCommandsQueueSize;
extern u8 gOutgoingCommandsQueueCount;
extern bool8 gIsOutgoingCommandsQueueReady;
extern u8 gOutgoingCommandsQueue[1024];

static bool8 sIsAuthenticated;
static bool8 sHasSentAuthRequest;
static u8 sCancellationReason;
static enum TcpState sTcpState;

void CreateTcpTask(void);
void Tcp_SerialCallback(void);

#endif // GUARD_LIBRFU_H