#include "tcp.h"
#include "task.h"
#include "malloc.h"
#include "main.h"
#include "gpu_regs.h"
#include "mmo.h"
#include "login_menu.h"

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

COMMON_DATA u8 gShouldAdvanceTcpState = 0;
EWRAM_DATA u16 gOutgoingCommandsQueueSize = 0;
EWRAM_DATA u8 gOutgoingCommandsQueueCount = 0;
EWRAM_DATA u8 gOutgoingCommandsQueue[1024] = {0};
EWRAM_DATA bool8 gIsOutgoingCommandsQueueReady = FALSE;

EWRAM_DATA u8 gIncomingTcpData[4096] = {0};
static u16 sExpectedIncomingByteCount;
static u16 sIncomingBytesReceived;

EWRAM_DATA u8 gOutgoingTcpData[1025] = {0};
static u16 sOutgoingTcpDataSize;
static u16 sOutgoingTcpSentBytes;
static bool8 sHasSentOutgoingHeader;

static u16 sCounter;

u32 Send(void);
void Receive(u32 inData);

static void EnableSerial32(void);
static void DisableSerial(void);

static void Task_Tcp(u8);

static void EnableSerial32(void)
{
    REG_RCNT = 0;
    REG_SIOCNT = SIO_32BIT_MODE | SIO_INTR_ENABLE;
    REG_SIOCNT |= SIO_MULTI_SD;
    REG_SIOCNT |= SIO_115200_BPS;
    REG_SIOCNT &= 0x7FF7;
    EnableInterrupts(INTR_FLAG_SERIAL);
    sIsAuthenticated = FALSE;
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
    EnableSerial32();

    sCounter = 0;
    sTcpState = TCP_STATE_HANDSHAKE;
    DestroyTask(taskId);
}

static void ProcessIncomingData()
{
    u8 commandCount, commandsProccessed;
    u8 commandType;
    u16 commandParamsLength;
    u8 shouldContinueReading;
    u8 i;
    u16 bytesRead;

    if (sIncomingBytesReceived < 3)
    {
        // not enough data to be a valid packet
        TcpLog("Invalid list of commands received.");
        memset(gIncomingTcpData, 0, sizeof(gIncomingTcpData));
        sExpectedIncomingByteCount = 0;
        sIncomingBytesReceived = 0;
        return;
    }

    // first byte should be the command count
    commandCount = gIncomingTcpData[0];
    if (commandCount == 0)
    {
        // no commands to process
        TcpLog("No commands to process.");
        memset(gIncomingTcpData, 0, sizeof(gIncomingTcpData));
        sExpectedIncomingByteCount = 0;
        sIncomingBytesReceived = 0;
        return;
    }

    shouldContinueReading = TRUE;
    bytesRead = 0;
    commandsProccessed = 0;
    while (shouldContinueReading)
    {
        if (bytesRead + 3 >= sIncomingBytesReceived - 1)
        {
            // not enough data to read the next command
            shouldContinueReading = FALSE;
            break;
        }

        commandType = gIncomingTcpData[1 + bytesRead];
        commandParamsLength = gIncomingTcpData[2 + bytesRead] | (gIncomingTcpData[3 + bytesRead] << 8);
        bytesRead += 3;

        if (bytesRead + commandParamsLength > sIncomingBytesReceived - 1)
        {
            // not enough data to read the command parameters
            shouldContinueReading = FALSE;
            break;
        }

        if (commandParamsLength > 0)
        {
            // process the command with parameters
            ProcessCommand(commandType, gIncomingTcpData + bytesRead + 1, commandParamsLength);
            bytesRead += commandParamsLength;
            commandsProccessed++;
        }
        else
        {
            // process the command without parameters
            ProcessCommand(commandType, NULL, 0);
            commandsProccessed++;
        }
    }

    if (commandsProccessed == commandCount)
    {
        // all commands have been processed
        TcpLog("All commands processed.");
    }
    else
    {
        // not all commands were processed
        TcpLogf("Not all commands were processed. %d of %d commands processed.", commandsProccessed, commandCount);
    }
}

u32 Send(void)
{
    u32 result = TCP_DATA_NOOP;
    u16 i;

    // if data is not currently being sent, copy the outgoing command queue to the outgoing data buffer
    if (sOutgoingTcpDataSize == 0)
    {
        // check to see if there is data to send
        if (gIsOutgoingCommandsQueueReady && gOutgoingCommandsQueueSize > 0 && gOutgoingCommandsQueueSize < sizeof(gOutgoingTcpData) - 1)
        {
            TcpLog("Sending new command data to SIO.");
            gOutgoingTcpData[0] = gOutgoingCommandsQueueCount;
            memcpy(gOutgoingTcpData + 1, gOutgoingCommandsQueue, gOutgoingCommandsQueueSize);
            sOutgoingTcpDataSize = gOutgoingCommandsQueueSize + 1;
            gOutgoingCommandsQueueSize = 0;
            gOutgoingCommandsQueueCount = 0;
            sOutgoingTcpSentBytes = 0;
            gIsOutgoingCommandsQueueReady = FALSE;
        }
    }

    // if there is data to send, send it
    if (sOutgoingTcpDataSize > 0)
    {
        if (!sHasSentOutgoingHeader)
        {
            result = TCP_SEND_TO_SIO | (sOutgoingTcpDataSize << 16);
            sHasSentOutgoingHeader = TRUE;
        }
        else if (sOutgoingTcpSentBytes < sOutgoingTcpDataSize)
        {
            result = 0;
            for (i = 0; i < sOutgoingTcpDataSize - sOutgoingTcpSentBytes; i++)
            {
                result |= gOutgoingTcpData[sOutgoingTcpSentBytes + i] << (i * 8);
                if (i == 3)
                {
                    break;
                }
            }
            sOutgoingTcpSentBytes += i + 1;

            if (sOutgoingTcpSentBytes >= sOutgoingTcpDataSize)
            {
                // all data has been sent, reset the outgoing data size
                sOutgoingTcpDataSize = 0;
                sOutgoingTcpSentBytes = 0;
                sHasSentOutgoingHeader = FALSE;
                memset(gOutgoingTcpData, 0, sizeof(gOutgoingTcpData));
                gOutgoingCommandsQueueSize = 0;
                gOutgoingCommandsQueueCount = 0;
                memset(gOutgoingCommandsQueue, 0, sizeof(gOutgoingCommandsQueue));
            }
        }
    }

    if (result != TCP_DATA_NOOP)
    {
        TcpLogf("Sending data: %08X", result);
    }

    return result;
}

void Receive(u32 inData)
{
    u8 incomingData[4];
    u16 bytesToRead;
    u8 i;

    if (inData == TCP_DATA_NOOP)
        return;

    if (sExpectedIncomingByteCount)
    {
        // the incoming 32 bit data should be four 8 bit bytes
        incomingData[0] = inData & 0xFF;
        incomingData[1] = (inData >> 8) & 0xFF;
        incomingData[2] = (inData >> 16) & 0xFF;
        incomingData[3] = (inData >> 24) & 0xFF;

        bytesToRead = sExpectedIncomingByteCount - sIncomingBytesReceived;
        if (bytesToRead > 4)
        {
            bytesToRead = 4;
        }

        for (i = 0; i < bytesToRead; i++)
        {
            gIncomingTcpData[sIncomingBytesReceived + i] = incomingData[i];
        }
        sIncomingBytesReceived += bytesToRead;
        if (sIncomingBytesReceived >= sExpectedIncomingByteCount)
        {
            // we have received all the data
            ProcessIncomingData();

            sExpectedIncomingByteCount = 0;
            sIncomingBytesReceived = 0;
            memset(gIncomingTcpData, 0, sizeof(gIncomingTcpData));
        }
    }
    else
    {
        // check the first 16 bits for the header
        u16 header = inData & 0xFFFF;
        u16 length = (inData >> 16) & 0xFFFF;
        if (header == TCP_RECEIVE_FROM_SIO)
        {
            sExpectedIncomingByteCount = length;
            sIncomingBytesReceived = 0;
        }
    }
}

void Tcp_SerialCallback(void)
{
    u32 recv32 = REG_SIODATA32;
    u16 header;
    u16 status;
    u32 url;
    u16 port;

    REG_IF = INTR_FLAG_SERIAL;
    //TcpLogf("Received Serial Interrupt! inValue %08X | state %d", recv32, sTcpState);
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

                sIsAuthenticated = FALSE;
                sHasSentAuthRequest = FALSE;
                sExpectedIncomingByteCount = 0;
                sIncomingBytesReceived = 0;
                sOutgoingTcpDataSize = 0;
                sHasSentOutgoingHeader = FALSE;
                gOutgoingCommandsQueueSize = 0;
                gOutgoingCommandsQueueCount = 0;

                Tcp_Connected();
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
            Receive(recv32);
            REG_SIODATA32 = Send();
            break;
        case TCP_STATE_DISCONNECTED:
            sIsAuthenticated = FALSE;
            sHasSentAuthRequest = FALSE;
            DisableSerial();
            break;
    }

    REG_SIOCNT &= 0x7FF7; // turn SO off so that we can receive signals again
}