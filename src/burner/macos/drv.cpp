// Driver Init module
#include "burner.h"

int bDrvOkay = 0;
char szAppRomPaths[DIRS_MAX][MAX_PATH] = {{"/usr/local/share/roms/"},{"roms/"}, };

static int DoLibInit()
{
    BzipOpen(false);
    int nRet = BurnDrvInit();
    BzipClose();

    return nRet != 0;
}

// Catch calls to BurnLoadRom() once the emulation has started;
// Intialise the zip module before forwarding the call, and exit cleanly.
static int DrvLoadRom(unsigned char* Dest, int* pnWrote, int i)
{
    BzipOpen(false);
    int nRet = BurnExtLoadRom(Dest, pnWrote, i);
    BzipClose();
    BurnExtLoadRom = DrvLoadRom;

    return nRet;
}

int DrvInit(int nDrvNum, bool bRestore)
{
    DrvExit();
    MediaExit();

    nBurnDrvSelect[0] = nDrvNum;
    nMaxPlayers = BurnDrvGetMaxPlayers();

    MediaInit();

    GameInpInit();

    ConfigGameLoad(true);
    InputMake(true);

    GameInpDefault();

    if (DoLibInit()) {
        BurnDrvExit();
        return 1;
    }

    BurnExtLoadRom = DrvLoadRom;
    bDrvOkay = 1;
    nBurnLayer = 0xFF; // show all layers

    RunReset();
    return 0;
}

int DrvInitCallback()
{
    return DrvInit(nBurnDrvSelect[0], false);
}

int DrvExit()
{
    if (bDrvOkay) {
        if (nBurnDrvSelect[0] < nBurnDrvCount) {
            ConfigGameSave(bSaveInputs);
            GameInpExit();
            BurnDrvExit();
        }
    }

    BurnExtLoadRom = NULL;
    bDrvOkay = 0;
    nBurnDrvSelect[0] = ~0U;

    return 0;
}
