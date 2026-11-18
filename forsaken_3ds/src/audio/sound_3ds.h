#pragma once

/* sound_3ds.h -- 3DS audio calls outside the sound.h seam: DSP firmware check
 *
 * ndspInit needs the DSP firmware at sdmc:/3ds/dspfirm.cdc. main_3ds.cpp
 * calls sound_check_dsp_firmware_available at boot, after gfxInit and before
 * citro3d takes the screens. If neither the SD nor the romfs copy exists,
 * sound_show_missing_firmware_warning prints dsp1 instructions on the top
 * screen (libctru console), blocks until A, START or SELECT, then restores
 * the top screen for citro3d; the game then runs without sound.
 */


bool sound_check_dsp_firmware_available(void);	/* SD or romfs copy present */
void sound_show_missing_firmware_warning(void);	/* modal; returns on A/START/SELECT */

