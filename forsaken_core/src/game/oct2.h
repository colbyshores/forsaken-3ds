#ifndef OCT2_INCLUDED
#define OCT2_INCLUDED

#include "game/main.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SHORT_LEVEL_NAME 32
extern char ShortLevelNames[MAXLEVELS][MAX_SHORT_LEVEL_NAME];

extern int16_t NewLevelNum;

#ifdef __cplusplus
}
#endif
#endif // OCT2_INCLUDED
