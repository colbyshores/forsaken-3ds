#pragma once

/* timer.h -- millisecond stopwatch measured in seconds
 *
 * A px_timer_t keeps the tick count (SDL_GetTicks; platform.h maps it to
 * platform_get_ticks on 3DS) of its last timer_run. timer_run returns the
 * seconds since that call and restarts the interval; while last is 0 (after
 * timer_clear, or a zero-initialised global) it only records the tick and
 * returns 0. timer_peek returns the same value as timer_run would without
 * changing the timer. millis holds the last interval in ms; best and worst
 * the shortest and longest intervals seen. timer_clear does not reset millis.
 *
 * State lives in the caller's struct: oct2.cpp's framelag and level timers,
 * ships.cpp's kill-memory and camping timers, secondary.cpp's mine timer and
 * so on.
 */

#include "game/main.h"
#ifndef __3DS__
#include <SDL.h>
#endif


typedef struct timer {
  u_int32_t  last;
  float   best;
  float   worst;
  float   seconds;
  u_int32_t  millis;
} px_timer_t;

float timer_run   ( px_timer_t* );
float timer_peek  ( px_timer_t* );
void  timer_clear ( px_timer_t* );
