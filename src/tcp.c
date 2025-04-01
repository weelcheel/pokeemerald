#include "tcp.h"
#include "task.h"
#include "malloc.h"
#include "main.h"

struct TcpTaskData
{
    u16 unused1;
    u16 unused2;
    u16 unused3;
    u16 unused4;
    u8 state;
    u8 unused5;
    u8 unused6;
    u8 unused7;
    bool8 unused8;
    bool8 unused9;
    u8 unused10;
    u8* unused11;
};

enum {
    TCP_STATE_INIT,
    TCP_STATE_HANDSHAKE,
    TCP_STATE_CONNECT,
    TCP_STATE_CONNECTING,
    TCP_STATE_CONNECTED,
    TCP_STATE_DISCONNECTED,
};

enum {
    TCP_CANCEL_TIMEOUT,
};

COMMON_DATA u8 gShouldAdvanceTcpState = 0;

static u16 sCounter;
static u8 sState;
static u8 sCancellationReason;

static void EnableSerial32(void);
static void DisableSerial(void);
static void EnableSio(void);

static void Task_Tcp(u8);

static void Tcp_Load(void);
static void Tcp_Connect(void);
static void Tcp_Reset(void);

static void EnableSerial32(void)
{
    REG_RCNT = 0;
    REG_SIOCNT = SIO_32BIT_MODE | SIO_INTR_ENABLE;
    REG_SIOCNT |= SIO_MULTI_SD;
    REG_SIOCNT |= SIO_38400_BPS;
    sCounter = 0;
    sState = 0;
}

static void DisableSerial(void)
{
    REG_IME = 0;
    REG_IE &= ~(INTR_FLAG_TIMER3 | INTR_FLAG_SERIAL);
    REG_IME = 1;
    REG_SIOCNT = 0;
    REG_TM3CNT_H = 0;
    REG_IF = INTR_FLAG_TIMER3 | INTR_FLAG_SERIAL;
}

static void EnableSio(void)
{
    REG_SIOCNT |= SIO_ENABLE;
}

void CreateTcpTask(void)
{
    struct TcpTaskData* data;
    u8 taskId = CreateTask(Task_Tcp, 0);
    data = (struct TcpTaskData *)gTasks[taskId].data;
    data->state = 0;
    data->unused1 = 0;
    data->unused2 = 0;
    data->unused3 = 0;
    data->unused4 = 0;
    data->unused5 = 0;
    data->unused6 = 0;
    data->unused7 = 0;
    data->unused8 = 0;
    data->unused9 = 0;
    data->unused10 = 0;
    data->unused11 = AllocZeroed(64);
    sState = 0;
}

static void Task_Tcp(u8 taskId)
{
    struct TcpTaskData* data = (struct TcpTaskData*)gTasks[taskId].data;
    switch (data->state)
    {
        case TCP_STATE_INIT:
            DebugPrint("Initializing...");
            Tcp_Load();
            EnableSerial32();
            EnableSio();

            sCounter = 0;
            data->state = TCP_STATE_HANDSHAKE;
            DebugPrint("Sending handshake...");
            break;
        case TCP_STATE_HANDSHAKE:
            sCounter++;
            if (sCounter > 60)
            {
                sCancellationReason = TCP_CANCEL_TIMEOUT;
                data->state = TCP_STATE_DISCONNECTED;
                DebugPrint("Handshake timed out!");
            }
            break;
        case TCP_STATE_CONNECT:
            Tcp_Connect();

            sCounter = 0;
            data->state = TCP_STATE_CONNECTING;
            DebugPrint("Waiting for successful connection...");
            break;
        case TCP_STATE_CONNECTING:
            sCounter++;
            if (sCounter > 60)
            {
                sCancellationReason = TCP_CANCEL_TIMEOUT;
                data->state = TCP_STATE_DISCONNECTED;
                DebugPrint("Waiting for connection timed out!");
            }
            break;
        case TCP_STATE_CONNECTED:
            break;
        case TCP_STATE_DISCONNECTED:
            DisableSerial();
            break;
    }

    sState = data->state;
}

void Tcp_SerialCallback(void)
{
    u16 i, cnt1, cnt2;
    u32 recv32;
    u16 recv[2];

    switch (sState)
    {
        case TCP_STATE_HANDSHAKE:
            // check to see received handshake
            *(u32*)recv = REG_SIODATA32;
            TcpLogf("Checking handshake: %d", REG_SIODATA32);
            for (i = 0, cnt1 = 0, cnt2 = 0; i < 2; i++)
            {
                if (recv[i] == TCP_HANDSHAKE)
                {
                    cnt1++;
                }
                else if (recv[i] != 0xFFFF)
                {
                    cnt2++;
                }
            }
            if (cnt1 == 2 && cnt2 == 0)
            {
                sState = TCP_STATE_CONNECT;
                TcpLog("Handshake successful!");
            }

            // send the handshake
            REG_SIODATA32 = TCP_HANDSHAKE;
            break;
        case TCP_STATE_CONNECTING:
            *(u32*)recv = REG_SIODATA32;
            for (i = 0, cnt1 = 0, cnt2 = 0; i < 2; i++)
            {
                if (recv[i] == TCP_CONNECTED)
                {
                    cnt1++;
                }
                else if (recv[i] != 0xFFFF)
                {
                    cnt2++;
                }
            }
            if (cnt1 == 2 && cnt2 == 0)
            {
                sState = TCP_STATE_CONNECTED;
                TcpLog("Connected successfully!");
            }
            break;
        case TCP_STATE_CONNECTED:
            break;
    }
}

static void Tcp_Connect(void)
{

}

static void Tcp_Load(void)
{
    volatile u16 backupIME = REG_IME;
    REG_IME = 0;
    gIntrTable[1] = Tcp_SerialCallback;
    REG_IE |= INTR_FLAG_VCOUNT;
    REG_IME = backupIME;
}

static void Tcp_Reset(void)
{
    volatile u16 backupIME = REG_IME;
    REG_IME = 0;
    RestoreSerialTimer3IntrHandlers();
    REG_IME = backupIME;
}