#pragma once

/*===================================================================
	sound_3ds -- 3DS-only audio services outside the sound.h seam.

	The DSP firmware check runs at boot, before ndsp is initialised:
	without sdmc:/3ds/dspfirm.cdc ndspInit fails and the game would be
	silent with no explanation, so main_3ds shows a warning screen.
===================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

bool sound_check_dsp_firmware_available(void);	/* SD or romfs copy present */
void sound_show_missing_firmware_warning(void);	/* modal; returns on A/START/SELECT */

#ifdef __cplusplus
}
#endif
