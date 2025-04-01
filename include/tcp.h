#ifndef GUARD_TCP_H
#define GUARD_TCP_H

#define TCP_HANDSHAKE   0xBAC7
#define TCP_CONNECTED   0xBAC8

#include "global.h"

extern u8 gShouldAdvanceTcpState;

void CreateTcpTask(void);
void Tcp_SerialCallback(void);

#endif // GUARD_LIBRFU_H