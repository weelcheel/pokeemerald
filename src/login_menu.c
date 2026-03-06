#include "login_menu.h"
#include "tcp.h"
#include "main.h"
#include "task.h"
#include "palette.h"
#include "mmo.h"
#include "naming_screen.h"
#include "global.h"
#include "constants/characters.h"
#include "constants/rgb.h"
#include "window.h"
#include "bg.h"
#include "gpu_regs.h"
#include "sprite.h"
#include "text.h"

#define LoginLog(pBuf) DebugPrint("[MMO LOG]: Login - " pBuf)
#define LoginLogf(pBuf, ...) DebugPrintf("[MMO LOG]: Login - " pBuf, __VA_ARGS__)

static void MainCB2(void);
static void Task_AuthFlow(u8 taskId);
static void CB2_ReturnFromUsernameScreen(void);
static void CB2_ReturnFromPasswordScreen(void);
static void SendAuthWithToken(void);
static void SendAuthWithUserPass(void);
static u8 GbaCharToAscii(u8 gbaChar);
static u8 GbaStringToAscii(const u8 *gbaStr, u8 *asciiStr);
static void SetupAuthScreen(void);
static void VBlankCB_Auth(void);

EWRAM_DATA static u8 sLoginUsername[LOGIN_NAME_LENGTH + 1] = {0};
EWRAM_DATA static u8 sLoginPassword[LOGIN_NAME_LENGTH + 1] = {0};
EWRAM_DATA u8 gAuthState = AUTH_STATE_NONE;

static const u8 sText_Authenticating[] = _("Authenticating...");

static const struct BgTemplate sAuthBgTemplates[] = {
    {
        .bg = 0,
        .charBaseIndex = 2,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
    },
};

static const struct WindowTemplate sAuthWindowTemplates[] = {
    {
        .bg = 0,
        .tilemapLeft = 7,
        .tilemapTop = 9,
        .width = 17,
        .height = 2,
        .paletteNum = 15,
        .baseBlock = 1,
    },
    DUMMY_WIN_TEMPLATE,
};

void CB2_InitTcp(void)
{
    gAuthState = AUTH_STATE_NONE;
    CreateTcpTask();
    CreateTask(Task_AuthFlow, 1);
    SetMainCallback2(MainCB2);
    SetSerialCallback(Tcp_SerialCallback);
}

void Tcp_Connected(void)
{
    LoginLog("TCP connected.");
    if (gSaveBlock1Ptr->authTokenLength > 0)
    {
        LoginLog("Saved JWT found, sending token auth.");
        SendAuthWithToken();
        gAuthState = AUTH_STATE_JWT_SENT;
    }
    else
    {
        LoginLog("No saved JWT, requesting login.");
        gAuthState = AUTH_STATE_NEEDS_LOGIN;
    }
}

void Tcp_Authenticated(void)
{
    LoginLog("Authenticated! Transitioning to game.");
    gAuthState = AUTH_STATE_AUTHENTICATED;
    SetMainCallback2(gMain.savedCallback);
}

void Tcp_AuthFailed(void)
{
    LoginLog("Auth failed, requesting login.");
    if (gAuthState == AUTH_STATE_JWT_SENT)
    {
        gSaveBlock1Ptr->authTokenLength = 0;
        memset(gSaveBlock1Ptr->authToken, 0, AUTH_TOKEN_MAX_LENGTH);
    }
    gAuthState = AUTH_STATE_NEEDS_LOGIN;
}

static void Task_AuthFlow(u8 taskId)
{
    if (gAuthState == AUTH_STATE_NEEDS_LOGIN)
    {
        gAuthState = AUTH_STATE_USERNAME_INPUT;
        memset(sLoginUsername, 0, sizeof(sLoginUsername));
        memset(sLoginPassword, 0, sizeof(sLoginPassword));
        FreeAllWindowBuffers();
        DestroyTask(taskId);
        DoNamingScreen(NAMING_SCREEN_USERNAME, sLoginUsername, 0, 0, 0, CB2_ReturnFromUsernameScreen);
    }
}

static void CB2_ReturnFromUsernameScreen(void)
{
    DoNamingScreen(NAMING_SCREEN_PASSWORD, sLoginPassword, 0, 0, 0, CB2_ReturnFromPasswordScreen);
}

static void CB2_ReturnFromPasswordScreen(void)
{
    SendAuthWithUserPass();
    gAuthState = AUTH_STATE_USERPASS_SENT;
    LoginLog("Credentials sent, waiting for auth response...");
    SetupAuthScreen();
    CreateTask(Task_AuthFlow, 1);
    SetMainCallback2(MainCB2);
}

static void SendAuthWithToken(void)
{
    SendCommand(COMMAND_AUTH, gSaveBlock1Ptr->authToken, gSaveBlock1Ptr->authTokenLength);
}

static void SendAuthWithUserPass(void)
{
    u8 asciiBuffer[LOGIN_NAME_LENGTH * 2 + 2];
    u8 len;

    len = GbaStringToAscii(sLoginUsername, asciiBuffer);
    asciiBuffer[len] = ':';
    len++;
    len += GbaStringToAscii(sLoginPassword, asciiBuffer + len);

    SendCommand(COMMAND_AUTH_USERPASS, asciiBuffer, len);
}

static u8 GbaCharToAscii(u8 gbaChar)
{
    if (gbaChar >= CHAR_A && gbaChar <= CHAR_Z)
        return 'A' + (gbaChar - CHAR_A);
    if (gbaChar >= CHAR_a && gbaChar <= CHAR_z)
        return 'a' + (gbaChar - CHAR_a);
    if (gbaChar >= CHAR_0 && gbaChar <= CHAR_9)
        return '0' + (gbaChar - CHAR_0);
    return 0;
}

static u8 GbaStringToAscii(const u8 *gbaStr, u8 *asciiStr)
{
    u8 i = 0;
    while (gbaStr[i] != EOS && i < LOGIN_NAME_LENGTH)
    {
        asciiStr[i] = GbaCharToAscii(gbaStr[i]);
        i++;
    }
    return i;
}

static void VBlankCB_Auth(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void SetupAuthScreen(void)
{
    SetVBlankCallback(NULL);
    DmaFill16(3, 0, VRAM, VRAM_SIZE);
    DmaFill32(3, 0, OAM, OAM_SIZE);
    DmaFill16(3, 0, PLTT, PLTT_SIZE);
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sAuthBgTemplates, ARRAY_COUNT(sAuthBgTemplates));
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetTasks();
    ResetPaletteFade();
    InitWindows(sAuthWindowTemplates);
    DeactivateAllTextPrinters();
    FillWindowPixelBuffer(0, PIXEL_FILL(0));
    gPlttBufferUnfaded[BG_PLTT_ID(15) + 1] = RGB_WHITE;
    gPlttBufferFaded[BG_PLTT_ID(15) + 1] = RGB_WHITE;
    AddTextPrinterParameterized(0, FONT_NORMAL, sText_Authenticating, 0, 1, 0, NULL);
    PutWindowTilemap(0);
    CopyWindowToVram(0, COPYWIN_FULL);
    ShowBg(0);
    SetVBlankCallback(VBlankCB_Auth);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
}

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}