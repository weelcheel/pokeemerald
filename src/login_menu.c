#include "login_menu.h"
#include "tcp.h"
#include "main.h"
#include "task.h"
#include "palette.h"

static void MainCB2(void);

void CB2_InitTcp(void)
{
    CreateTcpTask();
    SetMainCallback2(MainCB2);
    SetSerialCallback(Tcp_SerialCallback);
}

void Tcp_Connected(void)
{
    
}

void Tcp_Authenticated(void)
{
    SetMainCallback2(gMain.savedCallback);
}

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}