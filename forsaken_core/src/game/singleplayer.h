#pragma once

/* singleplayer.h -- title-menu entry points that start or load a single-player game
 *
 * singleplayer.cpp sets up a one-player host session and sets MyGameStatus;
 * the oct2.cpp state machine does the loading. StartASinglePlayerGame is also
 * the next-level transition (called with NULL), so both functions reload the
 * single-player level list only when it is not already active: InitLevels
 * resets NewLevelNum.
 */

bool LoadASinglePlayerGame( MENUITEM * Item );
bool StartASinglePlayerGame( MENUITEM * Item );
