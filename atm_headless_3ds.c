/* atm_headless_3ds.c — headless launch for ATM harness builds.
 * Compiles to nothing unless ATM_PROFILING is defined, so manual
 * (hbmenu) builds are unaffected by this file being present.
 *
 * ATM deploys and launches the .3dsx while HOME stays foreground, so NS
 * never delivers the APT wakeup that libctru's default __appInit blocks on
 * (the process parks in aptWaitForWakeUp forever and no game code runs).
 * Harness builds therefore override __appInit: bring up only what the game
 * needs (srv / HID / FS / SDMC) and skip aptInit entirely.
 *
 * Deliberately no APT session: APT_CheckNew3DS then fails closed to
 * is_n3ds=false, which is correct on the Old-3DS target (LOMEM tier,
 * 268 MHz, OG control defaults). N3DS-under-ATM would misdetect and is out
 * of scope for harness runs.
 *
 * Chainload ONLY uses this override when ATM_HEADLESS is also defined.
 * The stub-chainload path (3dslink push to the live stub, the only
 * supported game-delivery path since 2026-11-09) launches the app as a
 * proper NS foreground title, so harness builds use the normal libctru
 * init there — proven by the framedump render-proof builds. ATM_HEADLESS
 * preserves the old ATM-launch configuration for one-shot payloads.
 */
#if defined(ATM_PROFILING) && defined(ATM_HEADLESS)

#include <3ds.h>

void __appInit(void)
{
	srvInit();
	hidInit();
	fsInit();
	archiveMountSdmc();
}

#endif /* ATM_PROFILING && ATM_HEADLESS */
