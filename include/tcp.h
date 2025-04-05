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

#define TcpLog(pBuf) DebugPrint("[TCP LOG]: " pBuf)
#define TcpLogf(pBuf, ...) DebugPrintf("[TCP LOG]: " pBuf, __VA_ARGS__)

extern u8 gShouldAdvanceTcpState;

void CreateTcpTask(void);
void Tcp_SerialCallback(void);

#endif // GUARD_LIBRFU_H