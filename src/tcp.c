#include "tcp.h"
#include "task.h"
#include "malloc.h"
#include "main.h"
#include "gpu_regs.h"

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

COMMON_DATA u8 gShouldAdvanceTcpState = 0;

static u16 sCounter;
static u8 sCancellationReason;
static enum TcpState sTcpState;

static void EnableSerial32(void);
static void DisableSerial(void);

static void Task_Tcp(u8);
static void Tcp_Load(void);

static void EnableSerial32(void)
{
    REG_RCNT = 0;
    REG_SIOCNT = SIO_32BIT_MODE | SIO_INTR_ENABLE;
    REG_SIOCNT |= SIO_MULTI_SD;
    REG_SIOCNT |= SIO_115200_BPS;
    REG_SIOCNT &= 0x7FF7;
    EnableInterrupts(INTR_FLAG_SERIAL);
    sCounter = 0;
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

    TcpLog("Created TCP task!");
}

static void Task_Tcp(u8 taskId)
{
    TcpLog("Initializing...");
    Tcp_Load();
    EnableSerial32();

    sCounter = 0;
    sTcpState = TCP_STATE_HANDSHAKE;
    DestroyTask(taskId);
}

void Tcp_SerialCallback(void)
{
    u32 recv32 = REG_SIODATA32;
    u16 header;
    u16 status;
    u32 url;
    u16 port;

    REG_IF = INTR_FLAG_SERIAL;
    TcpLogf("Received Serial Interrupt! inValue %08X | state %d", recv32, sTcpState);
    switch (sTcpState)
    {
        case TCP_STATE_HANDSHAKE:
            sCounter++;    
            // check to see received handshake
            if (recv32 == (TCP_HANDSHAKE_SUCCESS)) 
            {
                sTcpState = TCP_STATE_INIT_URL_META;
                REG_SIODATA32 = TCP_HANDSHAKE_SUCCESS;
                TcpLog("Handshake successful!");
            }
            else if (sCounter > 60)
            {
                sCancellationReason = TCP_CANCEL_TIMEOUT;
                sTcpState = TCP_STATE_DISCONNECTED;
                TcpLog("Handshake timed out!");
            }
            else
            {
                REG_SIODATA32 = TCP_HANDSHAKE;
            }
            break;
        case TCP_STATE_INIT_URL_META:
            if (recv32 == (TCP_URL_META_SUCCESS))
            {
                TcpLog("Ready to send IP address!");
                sTcpState = TCP_STATE_INIT_URL_TRANSFER;
                
                url = 2130706433; // local host
                REG_SIODATA32 = url;
            }
            else 
            {
                REG_SIODATA32 = TCP_URL_META;
            }
            break;
        case TCP_STATE_INIT_URL_TRANSFER:
            if (recv32 == (TCP_URL_SUCCESS))
            {
                sTcpState = TCP_STATE_PORT_TRANSFER;
                TcpLog("IP address transferred!");

                port = 6969; // HTTP port
                REG_SIODATA32 = port;
            }
            break;
        case TCP_STATE_PORT_TRANSFER:
            if (recv32 == (TCP_PORT_SUCCESS))
            {
                sTcpState = TCP_STATE_CONNECTING;
                REG_SIODATA32 = TCP_PORT_SUCCESS;
                TcpLog("Port transferred!");
            }
            break;
        case TCP_STATE_CONNECTING:
            // break recv32 into 2 16 bit integers
            header = recv32 & 0xFFFF;
            status = (recv32 >> 16) & 0xFFFF;

            if (header == TCP_CONNECT_HEADER && recv32 == (TCP_CONNECT_SUCCESS))
            {
                TcpLog("Connected!");
                sTcpState = TCP_STATE_CONNECTED;
                REG_SIODATA32 = TCP_CONNECT_SUCCESS;
            }
            else if (header == TCP_DATA_FAILURE)
            {
                TcpLogf("Connection failed! Reason: %d", status);
                sCancellationReason = TCP_CANCEL_CONNECTION_FAILED;
                sTcpState = TCP_STATE_DISCONNECTED; 
                REG_SIODATA32 = TCP_DATA_NOOP;
            }
            break;
        case TCP_STATE_CONNECTED:
            break;
        case TCP_STATE_DISCONNECTED:
            DisableSerial();
            break;
    }

    REG_SIOCNT &= 0x7FF7; // turn SO off so that we can receive signals again
}

static void Tcp_Load(void)
{
    // volatile u16 backupIME = REG_IME;
    // REG_IME = 0;
    // gIntrTable[1] = Tcp_SerialCallback;
    // REG_IE |= INTR_FLAG_VCOUNT;
    // REG_IME = backupIME;
}