#pragma once

#include <stdint.h>

/* debugview.h -- the debug overlay and visualization switches
 *
 * Plain struct, public fields, no methods; one global instance, DebugViews,
 * defined in ui/title.cpp. The debug menus (title.cpp) keep raw pointers to
 * the fields (&DebugViews.ShowTrigZones and so on), so the field types must
 * stay as they are. Debug (the debug-output switch, on until the command line
 * or the 3DS startup clears it) and DebugLog (command-line log-file switch)
 * are the last two fields, so the older field offsets did not move.
 */
struct DebugView
{
	int16_t	ShowPortal = 0;
	bool	ShowTrigZones = false;
	bool	ShowEFZones = false;
	bool	ShowTeleports = false;
	bool	ShowBoundingBoxes = false;
	bool	ShowStartPoints = false;
	bool	ShowUntriggeredNMEs = false;
	bool	ShowColZones = false;
	bool	DebugInfo = false;
	bool	ShowNode = false;
	bool	ShowPlaneRGB = false;
	bool	NodeCube = false;
	bool	OldNodeCube = false;
	bool	NodeCubeType = false;
	bool	ShowFrameRate = false;
	bool	ShowInfo = false;
	bool	Debug = true;
	bool	DebugLog = false;
};

extern DebugView DebugViews;
