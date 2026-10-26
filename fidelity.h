/* fidelity.h — deterministic gameplay-state checksum for A/B fidelity proof.
 *
 * Compiled in only with -DFIDELITY_DUMP. Each simulation tick folds a
 * running FNV-1a hash over the gameplay-relevant state (ship/enemy
 * positions, angles, groups, shields, damage, bullet population, and the
 * player's nearest-node position). One line per level is written to
 * sdmc:/forsaken_fidelity.txt at the level marker. Two builds (baseline vs
 * candidate) that produce byte-identical fidelity files across a full
 * autotest sweep have provably identical gameplay behavior for the sweep's
 * scripted inputs — the gate required before shipping optimizations that
 * touch gameplay math (e.g. squared-distance node selection).
 *
 * Float hashing normalizes -0.0 to +0.0 so sign-of-zero noise cannot
 * produce false mismatches. Pointer values are NEVER hashed (heap layout
 * is not a gameplay property); node identity is hashed via node position.
 */
#ifndef FIDELITY_INCLUDED
#define FIDELITY_INCLUDED

#ifdef FIDELITY_DUMP

void fidelity_tick(void);
void fidelity_level(const char *level_name);
unsigned long fidelity_level_ticks(void);

#else

#define fidelity_tick() ((void)0)
#define fidelity_level(s) ((void)0)

#endif

#endif
