#include "ui/title.h"
#include "rendering/text.h"
/*===================================================================
	controls_3ds.cpp — bindings + persistence + rebind UI.
	See controls_3ds.h for API contract.
===================================================================*/

#ifdef __3DS__

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "platform_3ds/controls_3ds.h"

#define CFG_DIR  "sdmc:/3ds/forsaken"
#define CFG_PATH "sdmc:/3ds/forsaken/controls.cfg"

static controls_3ds_t s_controls;
static bool s_defaults_is_n3ds = false;  /* platform captured at init, for Reset to Default */

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
	"move_forward",
	"move_backward",
	"strafe_left",
	"strafe_right",
	"headlights",
	"rear_view",
	"nitro",
	"drop_mine",
	"next_primary",
	"prev_primary",
	"next_secondary",
	"prev_secondary",
	"slide_up",
	"slide_down",
};

/* Human-readable labels used by the rebind menu. */
static const char * const k_action_labels[CTRL_NUM_ACTIONS] = {
	"Fire Primary",
	"Fire Secondary",
	"Thrust Forward",
	"Thrust Back",
	"Strafe Left",
	"Strafe Right",
	"Headlights",
	"Rear View",
	"Nitro / Turbo",
	"Drop Mine",
	"Next Weapon",
	"Prev Weapon",
	"Next Special",
	"Prev Special",
	"Slide Up",
	"Slide Down",
};

static void apply_defaults(bool is_n3ds)
{
	/* Clear every slot first so the weapon-switch (10-13), slide (14-15)
	 * and optional Headlights (6) rows default to unbound. */
	memset(s_controls.button, 0, sizeof(s_controls.button));

	s_controls.button[CTRL_FIRE_PRIMARY]   = KEY_A;
	s_controls.button[CTRL_FIRE_SECONDARY] = KEY_B;
	s_controls.button[CTRL_MOVE_FORWARD]   = KEY_X;
	s_controls.button[CTRL_MOVE_BACK]      = KEY_Y;
	s_controls.button[CTRL_STRAFE_LEFT]    = KEY_L;
	s_controls.button[CTRL_STRAFE_RIGHT]   = KEY_R;
	/* Headlights (slot 6) unbound by default — START always opens the
	 * pause menu via the engine's own ESCAPE path, independent of any
	 * joystick binding, so this slot is a free optional bind. */
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
	/* Weapon-switch (Next/Prev Primary+Special) and Slide Up/Down are
	 * unbound by default: the D-pad already drives them through the POV
	 * hat (config.cpp binds POV left/right = prev/next primary, up/down =
	 * slide down/up). These rows let the player additionally map them to
	 * face buttons. */

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

	s_defaults_is_n3ds = is_n3ds;
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

void controls_3ds_reset_defaults(void)
{
	apply_defaults(s_defaults_is_n3ds);
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
	Hooked menu — replaces the old modal loop.

	State machine lives in two functions called from the engine:
	  controls_3ds_render_overlay() runs per-frame from the bottom-
		screen draw block (oct2.cpp), emits text via Print4x5Text.
	  controls_3ds_handle_input() runs at the top of handle_events,
		consumes navigation/rebind input. Returns true to suppress
		normal input propagation while menu is up.
	No modal loop, no consoleInit, no DisplayTransfer dance.
===================================================================*/

/* C-stick rows used to live here (axis routing + inversion). Removed
 * from the UI because c-stick isn't bound in the engine's mapping
 * path — toggling them did nothing visible. cstick_* fields stay
 * in the struct + cfg file for forwards compatibility.
 *
 * ROW_AIM_ASSIST is a non-action row — DPAD LEFT/RIGHT adjusts the
 * Config.aim_assist_strength float in 0.05 steps (0.00 to 1.00).
 * On Save & Exit we also persist via write_config so the value
 * survives across sessions. */
#define ROW_AIM_ASSIST    (CTRL_NUM_ACTIONS + 0)
#define ROW_RESET         (CTRL_NUM_ACTIONS + 1)
#define ROW_SAVE          (CTRL_NUM_ACTIONS + 2)
#define ROW_CANCEL        (CTRL_NUM_ACTIONS + 3)
#define MENU_ROWS         (CTRL_NUM_ACTIONS + 4)

/* Bridge to USERCONFIG so the aim-assist row reads/writes the same
 * value the engine uses at runtime. write_config persists it to the
 * main config file (Configs/main.txt). */
#include "data/config.h"

constexpr float AIM_ASSIST_STEP = 0.05F;
constexpr float AIM_ASSIST_MIN = 0.00F;
constexpr float AIM_ASSIST_MAX = 1.00F;
static float s_aim_assist_backup = 0.0F;

static float aim_assist_current(void)
{
	return player_config ? player_config->aim_assist_strength : 0.0F;
}

static void aim_assist_adjust(float delta)
{
	if (!player_config) return;
	float v = player_config->aim_assist_strength + delta;
	if (v < AIM_ASSIST_MIN) v = AIM_ASSIST_MIN;
	if (v > AIM_ASSIST_MAX) v = AIM_ASSIST_MAX;
	player_config->aim_assist_strength = v;
}

static bool s_menu_open = false;
static int  s_selected = 0;
static bool s_rebinding = false;
static controls_3ds_t s_backup;

bool controls_3ds_menu_is_open(void) { return s_menu_open; }

void controls_3ds_render_overlay(void)
{
	if (!s_menu_open) return;

	const int FG = 7;        /* white */
	const int FG_HL = 4;     /* yellow-ish (for selected row) */
	int y = 6;

	Print4x5Text("FORSAKEN CONTROLS", 20, y, FG);
	y += 12;
	if (s_rebinding) {
		Print4x5Text("PRESS A BUTTON TO BIND", 20, y, FG_HL);
	} else {
		Print4x5Text("DPAD: SELECT  A: SET", 20, y, FG);
	}
	y += 14;

	for (int i = 0; i < CTRL_NUM_ACTIONS; i++) {
		int color = (i == s_selected) ? FG_HL : FG;
		char line[64];
		snprintf(line, sizeof(line), "%s %-15s %s",
				 (i == s_selected) ? ">" : " ",
				 k_action_labels[i],
				 button_name(s_controls.button[i]));
		Print4x5Text(line, 8, y, color);
		y += 10;
	}

	/* Aim Assist row — non-action, adjusts a float with DPAD LEFT/RIGHT. */
	{
		char line[64];
		int pct = (int)(aim_assist_current() * 100.0F + 0.5F);
		int color = (s_selected == ROW_AIM_ASSIST) ? FG_HL : FG;
		snprintf(line, sizeof(line), "%s %-15s <%3d%%>",
				 (s_selected == ROW_AIM_ASSIST) ? ">" : " ",
				 "Aim Assist",
				 pct);
		Print4x5Text(line, 8, y, color);
		y += 10;
	}

	Print4x5Text((s_selected == ROW_RESET)  ? "> [RESET TO DEFAULT]" : "  [RESET TO DEFAULT]",
				 8, y, (s_selected == ROW_RESET) ? FG_HL : FG); y += 10;
	Print4x5Text((s_selected == ROW_SAVE)   ? "> [SAVE & EXIT]" : "  [SAVE & EXIT]",
				 8, y, (s_selected == ROW_SAVE) ? FG_HL : FG); y += 10;
	Print4x5Text((s_selected == ROW_CANCEL) ? "> [CANCEL]"      : "  [CANCEL]",
				 8, y, (s_selected == ROW_CANCEL) ? FG_HL : FG);
}

bool controls_3ds_handle_input(u32 kDown, u32 kHeld)
{
	(void)kHeld;
	if (!s_menu_open) return false;

	if (s_rebinding) {
		/* Wait for any single hardware button to commit the bind.
		 * START aborts (no change). Touchscreen would clear (when
		 * we add it) — for now plain hardware-button only. */
		if (kDown & KEY_START) {
			s_rebinding = false;
		} else {
			for (size_t i = 0; i < NUM_BUTTONS; i++) {
				if (kDown & k_buttons[i].mask) {
					s_controls.button[s_selected] = k_buttons[i].mask;
					s_rebinding = false;
					break;
				}
			}
		}
		return true;
	}

	if (kDown & KEY_DUP) {
		s_selected = (s_selected - 1 + MENU_ROWS) % MENU_ROWS;
	} else if (kDown & KEY_DDOWN) {
		s_selected = (s_selected + 1) % MENU_ROWS;
	} else if ((kDown & KEY_DLEFT) && s_selected == ROW_AIM_ASSIST) {
		aim_assist_adjust(-AIM_ASSIST_STEP);
	} else if ((kDown & KEY_DRIGHT) && s_selected == ROW_AIM_ASSIST) {
		aim_assist_adjust(+AIM_ASSIST_STEP);
	} else if (kDown & KEY_A) {
		if (s_selected < CTRL_NUM_ACTIONS) {
			s_rebinding = true;
		} else if (s_selected == ROW_RESET) {
			/* Restore platform defaults; stay in the menu so the
			 * change is visible. Persists only on Save & Exit. */
			controls_3ds_reset_defaults();
		} else if (s_selected == ROW_SAVE) {
			controls_3ds_save();
			if (player_config) write_config(player_config, biker_config);
			s_menu_open = false;
		} else if (s_selected == ROW_CANCEL) {
			s_controls = s_backup;
			if (player_config) player_config->aim_assist_strength = s_aim_assist_backup;
			s_menu_open = false;
		}
		/* A on ROW_AIM_ASSIST is a no-op — use LEFT/RIGHT to adjust. */
	} else if (kDown & KEY_START) {
		controls_3ds_save();
		if (player_config) write_config(player_config, biker_config);
		s_menu_open = false;
	} else if (kDown & KEY_B) {
		s_controls = s_backup;
		if (player_config) player_config->aim_assist_strength = s_aim_assist_backup;
		s_menu_open = false;
	}
	return true;
}

void controls_3ds_remap_menu(void)
{
	s_backup = s_controls;
	s_aim_assist_backup = aim_assist_current();
	s_selected = 0;
	s_rebinding = false;
	s_menu_open = true;
}

#endif /* __3DS__ */
