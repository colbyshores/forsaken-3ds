/*===================================================================
	controls_3ds.c — bindings + persistence + rebind UI.
	See controls_3ds.h for API contract.
===================================================================*/

#ifdef __3DS__

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "controls_3ds.h"

#define CFG_DIR  "sdmc:/3ds/forsaken"
#define CFG_PATH "sdmc:/3ds/forsaken/controls.cfg"

static controls_3ds_t s_controls;

/* Catalog of buttons the user can bind, in the order shown by the
 * rebind menu. Each entry pairs a stable name (used in the cfg
 * file) with the libctru KEY_* mask. */
typedef struct { const char *name; u32 mask; } button_entry_t;
static const button_entry_t k_buttons[] = {
	{ "A",       KEY_A      },
	{ "B",       KEY_B      },
	{ "X",       KEY_X      },
	{ "Y",       KEY_Y      },
	{ "L",       KEY_L      },
	{ "R",       KEY_R      },
	{ "ZL",      KEY_ZL     },
	{ "ZR",      KEY_ZR     },
	{ "START",   KEY_START  },
	{ "SELECT",  KEY_SELECT },
	{ "DUP",     KEY_DUP    },
	{ "DDOWN",   KEY_DDOWN  },
	{ "DLEFT",   KEY_DLEFT  },
	{ "DRIGHT",  KEY_DRIGHT },
};
#define NUM_BUTTONS (sizeof(k_buttons)/sizeof(k_buttons[0]))

static const char *button_name(u32 mask)
{
	if (mask == 0) return "(none)";
	for (size_t i = 0; i < NUM_BUTTONS; i++)
		if (k_buttons[i].mask == mask) return k_buttons[i].name;
	return "(unknown)";
}

static u32 button_mask_from_name(const char *name)
{
	for (size_t i = 0; i < NUM_BUTTONS; i++)
		if (strcasecmp(k_buttons[i].name, name) == 0)
			return k_buttons[i].mask;
	return 0;
}

/* Stable text keys for each action. Used as the LHS of every line
 * in controls.cfg. Order MUST match controls_action_t. */
static const char * const k_action_names[CTRL_NUM_ACTIONS] = {
	"fire_primary",
	"fire_secondary",
	"next_weapon",
	"prev_weapon",
	"strafe_left",
	"strafe_right",
	"menu",
	"rear_view",
	"nitro",
	"drop_mine",
};

/* Human-readable labels used by the rebind menu. */
static const char * const k_action_labels[CTRL_NUM_ACTIONS] = {
	"Fire Primary",
	"Fire Secondary",
	"Next Weapon",
	"Prev Weapon",
	"Strafe Left",
	"Strafe Right",
	"Pause Menu",
	"Rear View",
	"Nitro / Turbo",
	"Drop Mine",
};

static void apply_defaults(bool is_n3ds)
{
	s_controls.button[CTRL_FIRE_PRIMARY]   = KEY_A;
	s_controls.button[CTRL_FIRE_SECONDARY] = KEY_B;
	s_controls.button[CTRL_NEXT_WEAPON]    = KEY_X;
	s_controls.button[CTRL_PREV_WEAPON]    = KEY_Y;
	s_controls.button[CTRL_STRAFE_LEFT]    = KEY_L;
	s_controls.button[CTRL_STRAFE_RIGHT]   = KEY_R;
	s_controls.button[CTRL_MENU]           = KEY_START;
	if (is_n3ds) {
		s_controls.button[CTRL_REAR_VIEW]  = KEY_SELECT;
		s_controls.button[CTRL_NITRO]      = KEY_ZL;
		s_controls.button[CTRL_DROP_MINE]  = KEY_ZR;
	} else {
		/* OG 3DS: no ZL/ZR. Sacrifice rear view; SELECT becomes nitro
		 * (mandatory for SP level 3 progression). Drop mine unbound. */
		s_controls.button[CTRL_REAR_VIEW]  = 0;
		s_controls.button[CTRL_NITRO]      = KEY_SELECT;
		s_controls.button[CTRL_DROP_MINE]  = 0;
	}

	/* C-stick defaults: drive engine axes 2 (yaw) and 3 (pitch),
	 * non-inverted. Matches the historical behaviour before the
	 * remap system existed. */
	s_controls.cstick_axis_x   = 2;
	s_controls.cstick_axis_y   = 3;
	s_controls.cstick_invert_x = 0;
	s_controls.cstick_invert_y = 0;
}

static void parse_line(char *line)
{
	char *eq;
	char *key, *val;
	size_t i;

	/* Trim trailing CR/LF/whitespace. */
	for (size_t n = strlen(line); n > 0 && (line[n-1] == '\r' || line[n-1] == '\n' || line[n-1] == ' ' || line[n-1] == '\t'); n--)
		line[n-1] = '\0';

	/* Skip comments / empty. */
	if (line[0] == '#' || line[0] == '\0') return;

	eq = strchr(line, '=');
	if (!eq) return;
	*eq = '\0';
	key = line;
	val = eq + 1;
	/* Trim leading space on val. */
	while (*val == ' ' || *val == '\t') val++;

	for (i = 0; i < CTRL_NUM_ACTIONS; i++) {
		if (strcasecmp(key, k_action_names[i]) == 0) {
			s_controls.button[i] = button_mask_from_name(val);
			return;
		}
	}
	if      (strcasecmp(key, "cstick_axis_x")   == 0) s_controls.cstick_axis_x   = atoi(val);
	else if (strcasecmp(key, "cstick_axis_y")   == 0) s_controls.cstick_axis_y   = atoi(val);
	else if (strcasecmp(key, "cstick_invert_x") == 0) s_controls.cstick_invert_x = atoi(val) ? 1 : 0;
	else if (strcasecmp(key, "cstick_invert_y") == 0) s_controls.cstick_invert_y = atoi(val) ? 1 : 0;
}

void controls_3ds_init(bool is_n3ds)
{
	FILE *f;
	char line[128];

	apply_defaults(is_n3ds);

	f = fopen(CFG_PATH, "r");
	if (!f) return;  /* no save → defaults persist */
	while (fgets(line, sizeof(line), f))
		parse_line(line);
	fclose(f);
}

void controls_3ds_save(void)
{
	FILE *f;
	size_t i;

	mkdir(CFG_DIR, 0777);  /* harmless if already exists */
	f = fopen(CFG_PATH, "w");
	if (!f) return;

	fprintf(f, "# forsaken 3ds controls — edit by hand or use the in-game menu.\n");
	for (i = 0; i < CTRL_NUM_ACTIONS; i++) {
		fprintf(f, "%s=%s\n", k_action_names[i], button_name(s_controls.button[i]));
	}
	fprintf(f, "cstick_axis_x=%d\n",   s_controls.cstick_axis_x);
	fprintf(f, "cstick_axis_y=%d\n",   s_controls.cstick_axis_y);
	fprintf(f, "cstick_invert_x=%d\n", s_controls.cstick_invert_x);
	fprintf(f, "cstick_invert_y=%d\n", s_controls.cstick_invert_y);
	fclose(f);
}

u32 controls_3ds_button(controls_action_t action)
{
	if ((unsigned)action >= CTRL_NUM_ACTIONS) return 0;
	return s_controls.button[action];
}

int controls_3ds_cstick_axis_x(void)   { return s_controls.cstick_axis_x; }
int controls_3ds_cstick_axis_y(void)   { return s_controls.cstick_axis_y; }
int controls_3ds_cstick_invert_x(void) { return s_controls.cstick_invert_x; }
int controls_3ds_cstick_invert_y(void) { return s_controls.cstick_invert_y; }

/*===================================================================
	Bottom-screen rebind menu.

	Modal — takes over the bottom screen with consoleInit, runs its
	own input loop, returns when user picks "Save & Exit" or hits B
	at the top of the action list. Caller is responsible for
	restoring whatever it had on the bottom screen afterward.
===================================================================*/

#define MENU_ITEMS (CTRL_NUM_ACTIONS + 4 + 2)
/* layout: 10 actions + 4 c-stick rows + "Save & Exit" + "Cancel" */

/* On-screen touchable button zones (drawn during rebind mode).
 * Bottom screen is 320x240; console is 40x30 chars at 8x8 px each.
 * Reserve the last console row (row 29 = pixel y 232..239) for two
 * touchable rectangles: CLEAR (left half, x 0..159) and ABORT
 * (right half, x 160..319). Plain hardware buttons can't safely be
 * reserved as control keys here because every one of them can be
 * a player-rebound action. */
#define TOUCH_BAR_Y_PX  216    /* console row 27 */
#define TOUCH_BAR_H_PX  24     /* 3 rows */
#define TOUCH_MID_X     160

static void draw_touch_bar(void)
{
	/* Print the two virtual buttons on the last 3 rows. ANSI inverse
	 * video so they read as buttons against the surrounding text. */
	printf("\x1b[28;1H");                      /* row 28, col 1 */
	printf("\x1b[7m   [ CLEAR ]   \x1b[0m");   /* 14 chars left */
	printf("\x1b[7m   [ ABORT ]   \x1b[0m");   /* 14 chars right */
}

static void draw_menu(int selected, bool rebinding)
{
	consoleClear();
	printf("\x1b[1;1H\x1b[37mForsaken Controls\x1b[0m\n");
	printf("DPad: select   A: rebind   L/R: cycle\n");
	printf("START: save+exit   B: cancel\n");
	printf("------------------------------------\n");

	for (int i = 0; i < CTRL_NUM_ACTIONS; i++) {
		const char *cursor = (i == selected) ? ">" : " ";
		const char *hl     = (i == selected && rebinding) ? "\x1b[33m" : "";
		const char *rs     = (i == selected && rebinding) ? "\x1b[0m"  : "";
		printf("%s %-16s %s%s%s\n", cursor, k_action_labels[i], hl,
		       button_name(s_controls.button[i]), rs);
	}
	int row = CTRL_NUM_ACTIONS;
	printf("%s %-16s axis %d\n",
	       (selected == row+0) ? ">" : " ",
	       "C-stick X axis",
	       s_controls.cstick_axis_x);
	printf("%s %-16s axis %d\n",
	       (selected == row+1) ? ">" : " ",
	       "C-stick Y axis",
	       s_controls.cstick_axis_y);
	printf("%s %-16s %s\n",
	       (selected == row+2) ? ">" : " ",
	       "C-stick invert X",
	       s_controls.cstick_invert_x ? "on" : "off");
	printf("%s %-16s %s\n",
	       (selected == row+3) ? ">" : " ",
	       "C-stick invert Y",
	       s_controls.cstick_invert_y ? "on" : "off");

	printf("%s %s\n", (selected == CTRL_NUM_ACTIONS+4) ? ">" : " ", "[Save & Exit]");
	printf("%s %s\n", (selected == CTRL_NUM_ACTIONS+5) ? ">" : " ", "[Cancel]");

	if (rebinding && selected < CTRL_NUM_ACTIONS) {
		printf("\n  Press a hardware button to bind.\n");
		printf("  Touch screen for CLEAR / ABORT.\n");
		draw_touch_bar();
	}
}

void controls_3ds_remap_menu(void)
{
	PrintConsole console;
	int selected = 0;
	bool rebinding = false;
	controls_3ds_t backup = s_controls;

	consoleInit(GFX_BOTTOM, &console);

	/* Drain pending key state so the chord that opened the menu doesn't
	 * also count as the first navigation press. */
	hidScanInput(); hidKeysDown();

	draw_menu(selected, rebinding);

	for (;;) {
		hidScanInput();
		u32 kDown = hidKeysDown();

		if (rebinding) {
			/* Touchscreen handles CLEAR / ABORT (virtual buttons in
			 * the bar drawn at the bottom of the screen). Every
			 * hardware button — including A, B, SELECT, START —
			 * binds when pressed alone. */
			if (kDown & KEY_TOUCH) {
				touchPosition tp;
				hidTouchRead(&tp);
				if (tp.py >= TOUCH_BAR_Y_PX && tp.py < TOUCH_BAR_Y_PX + TOUCH_BAR_H_PX) {
					if (tp.px < TOUCH_MID_X) {
						s_controls.button[selected] = 0;  /* CLEAR */
					}
					/* right half = ABORT (no binding change) */
					rebinding = false;
				}
			} else {
				for (size_t i = 0; i < NUM_BUTTONS; i++) {
					if (kDown & k_buttons[i].mask) {
						s_controls.button[selected] = k_buttons[i].mask;
						rebinding = false;
						break;
					}
				}
			}
			if (!rebinding) draw_menu(selected, false);
		} else {
			if (kDown & KEY_DUP) {
				selected = (selected - 1 + MENU_ITEMS) % MENU_ITEMS;
				draw_menu(selected, false);
			} else if (kDown & KEY_DDOWN) {
				selected = (selected + 1) % MENU_ITEMS;
				draw_menu(selected, false);
			} else if (kDown & KEY_A) {
				if (selected < CTRL_NUM_ACTIONS) {
					rebinding = true;
					draw_menu(selected, true);
				} else {
					int row = CTRL_NUM_ACTIONS;
					if (selected == row+2) s_controls.cstick_invert_x ^= 1;
					else if (selected == row+3) s_controls.cstick_invert_y ^= 1;
					else if (selected == CTRL_NUM_ACTIONS+4) {
						controls_3ds_save();
						break;
					}
					else if (selected == CTRL_NUM_ACTIONS+5) {
						s_controls = backup;
						break;
					}
					draw_menu(selected, false);
				}
			} else if ((kDown & KEY_L) || (kDown & KEY_DLEFT)) {
				int row = CTRL_NUM_ACTIONS;
				if (selected == row+0)
					s_controls.cstick_axis_x = (s_controls.cstick_axis_x + 3) % 4;
				else if (selected == row+1)
					s_controls.cstick_axis_y = (s_controls.cstick_axis_y + 3) % 4;
				draw_menu(selected, false);
			} else if ((kDown & KEY_R) || (kDown & KEY_DRIGHT)) {
				int row = CTRL_NUM_ACTIONS;
				if (selected == row+0)
					s_controls.cstick_axis_x = (s_controls.cstick_axis_x + 1) % 4;
				else if (selected == row+1)
					s_controls.cstick_axis_y = (s_controls.cstick_axis_y + 1) % 4;
				draw_menu(selected, false);
			} else if (kDown & KEY_START) {
				controls_3ds_save();
				break;
			} else if (kDown & KEY_B) {
				s_controls = backup;
				break;
			}
		}

		gspWaitForVBlank();
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	/* Bottom-screen restore. consoleInit changes the screen format
	 * AND disables double-buffering. If we don't reset the format,
	 * gfxGetFramebuffer may return a buffer whose stride/size doesn't
	 * match what the display controller is actually scanning out —
	 * memset hits the "wrong" buffer and corruption persists.
	 *
	 * Order matters:
	 *   1. consoleClear() wipes the console character grid
	 *   2. gfxSetScreenFormat → BGR8 (libctru default, 3 bytes/pixel)
	 *   3. Re-enable double-buffering
	 *   4. swap+vblank to commit the format change
	 *   5. Four memset+swap+vblank cycles — covers both halves of
	 *      the swap chain twice, ensuring whichever buffer the
	 *      display controller latches onto is black. */
	consoleClear();
	gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
	gfxSetDoubleBuffering(GFX_BOTTOM, true);
	gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();

	for (int i = 0; i < 4; i++) {
		u16 fbw = 0, fbh = 0;
		u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fbw, &fbh);
		if (fb) memset(fb, 0, (size_t)fbw * (size_t)fbh * 3);
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
}

#endif /* __3DS__ */
