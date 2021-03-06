// burn_ym2151.h
#include "driver.h"
extern "C" {
 #include "ym2151.h"
}

#include "timer.h"

INT32 BurnYM2151Init(INT32 nClockFrequency);
INT32 BurnYM2151Init(INT32 nClockFrequency, INT32 use_timer);
void BurnYM2151SetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir);
void BurnYM2151Reset();
void BurnYM2151Exit();
void BurnYM2151Render(INT16* pSoundBuf, INT32 nSegmentLength);
void BurnYM2151Scan(INT32 nAction, INT32 *pnMin);
void BurnYM2151SetInterleave(INT32 nInterleave);

inline static void BurnYM2151Write(INT32 offset, const UINT8 nData)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151Write called without init\n"));
#endif

	extern UINT32 nBurnCurrentYM2151Register;

	if (offset & 1) {
		YM2151WriteReg(0, nBurnCurrentYM2151Register, nData);
	} else {
		nBurnCurrentYM2151Register = nData;
	}
}

static inline void BurnYM2151SelectRegister(const UINT8 nRegister)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151SelectRegister called without init\n"));
#endif

	extern UINT32 nBurnCurrentYM2151Register;

	nBurnCurrentYM2151Register = nRegister;
}

static inline void BurnYM2151WriteRegister(const UINT8 nValue)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151WriteRegister called without init\n"));
#endif

	extern UINT32 nBurnCurrentYM2151Register;

	YM2151WriteReg(0, nBurnCurrentYM2151Register, nValue);
}

#define BurnYM2151Read() YM2151ReadStatus(0)

#if defined FBNEO_DEBUG
	#define BurnYM2151SetIrqHandler(h) if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151SetIrqHandler called without init\n")); YM2151SetIrqHandler(0, h)
	#define BurnYM2151SetPortHandler(h) if (!DebugSnd_YM2151Initted) bprintf(PRINT_ERROR, _T("BurnYM2151SetPortHandler called without init\n")); YM2151SetPortWriteHandler(0, h)
#else
	#define BurnYM2151SetIrqHandler(h) YM2151SetIrqHandler(0, h)
	#define BurnYM2151SetPortHandler(h) YM2151SetPortWriteHandler(0, h)
#endif

#define BURN_SND_YM2151_YM2151_ROUTE_1		0
#define BURN_SND_YM2151_YM2151_ROUTE_2		1

#define BurnYM2151SetAllRoutes(v, d)							\
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, v, d);	\
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, v, d);
