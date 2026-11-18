
/* timer.cpp -- millisecond stopwatch; see timer.h. */
#include "game/main.h"
#include "util/platform.h"
#include "util/timer.h"
#include "util/util.h"

// seconds since the last timer_run, leaving the timer unchanged
float timer_peek( px_timer_t* stats )
{
	float seconds;
	px_timer_t old = *stats;
	seconds = timer_run( stats );
	*stats = old; // restore it
	return seconds;
}

// restart from scratch: the next timer_run only records the tick
void timer_clear( px_timer_t* stats )
{
	stats->last    = 0;
	stats->worst   = 0;
	stats->best    = 0;
	stats->seconds = 0.0F;
}

// seconds since the previous call; restarts the interval
float timer_run( px_timer_t* stats )
{

  u_int32_t time_now;

  // first run since timer_clear: just record the tick
  if(!stats->last)
  {

    stats->last = SDL_GetTicks();

    return 0.0F;

  }

  time_now = SDL_GetTicks();

  stats->millis = time_now - stats->last;

  stats->seconds = ((float)stats->millis) / 1000.0F; // 1000 ms == 1 s

  stats->last = time_now;

  if (stats->best == 0.0F || stats->seconds < stats->best)
    stats->best = stats->seconds;

  if (stats->worst  == 0.0F || stats->seconds > stats->worst)
    stats->worst = stats->seconds;

  return stats->seconds;

}
