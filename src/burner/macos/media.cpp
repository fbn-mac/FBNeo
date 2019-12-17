// Media module
#include "burner.h"

int MediaInit()
{
    if (!bInputOkay)
        InputInit();

    nAppVirtualFps = nBurnFPS;

    if (!bAudOkay)
        AudSoundInit();

    nBurnSoundRate = 0;
    pBurnSoundOut = NULL;
    if (bAudOkay) {
        nBurnSoundRate = nAudSampleRate[nAudSelect];
        nBurnSoundLen = nAudSegLen;
    }

    if (!bVidOkay) {
        // Reinit the video plugin
        VidInit();
        if (!bVidOkay && nVidFullscreen) {
            MediaExit();
            return MediaInit();
        }

        if (bVidOkay && (bRunPause || !bDrvOkay))
            VidRedraw();
    }

    return 0;
}

int MediaExit()
{
    nBurnSoundRate = 0; // Blank sound
    pBurnSoundOut = NULL;

    AudSoundExit();
    VidExit();
    InputExit();

    return 0;
}
