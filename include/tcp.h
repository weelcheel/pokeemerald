#ifndef GUARD_TCP_H
#define GUARD_TCP_H

#include "global.h"

#define TCP_HANDSHAKE   0xBAC7
#define TCP_CONNECTED   0xBAC8
#define TcpLog(pBuf) DebugPrint("[TCP LOG]: " pBuf)
#define TcpLogf(pBuf, ...) DebugPrintf("[TCP LOG]: " pBuf, __VA_ARGS__)

extern u8 gShouldAdvanceTcpState;

void CreateTcpTask(void);
void Tcp_SerialCallback(void);

#endif // GUARD_LIBRFU_H