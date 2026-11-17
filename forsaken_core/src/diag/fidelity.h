/* fidelity.h — deterministic gameplay-state checksum for A/B fidelity proof.
 *
 * Compiled in only with -DFIDELITY_DUMP. Each simulation tick folds a
 * running FNV-1a hash over the gameplay-relevant state (ship/enemy
 * positions, angles, groups, shields, damage, bullet population, and the
 * player's nearest-node position). One line per level is written to
 * sdmc:/3ds/forsaken_fidelity.txt at the level marker. Two builds (baseline vs
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
/* M1 render-purity probe: current gameplay-state hash (FNV-1a over ships,
 * enemies, bullets). A repeated render of the same tick must not move it. */
unsigned long fidelity_hash(void);

#ifdef FIDELITY_CHECKPOINTS
/* Emits a hash checkpoint every FIDELITY_CHECKPOINT_TICKS simulation ticks
 * (line "FIDC: level=<name> tick=<n> hash=<hex>") so a divergence between
 * two builds can be localized to the exact tick instead of only the level
 * boundary. Off by default: it changes the file format, so a checkpointed
 * file is only comparable with another checkpointed file. */
#ifndef FIDELITY_CHECKPOINT_TICKS
#define FIDELITY_CHECKPOINT_TICKS 250
#endif
#endif

#else

#define fidelity_tick() ((void)0)
#define fidelity_level(s) ((void)0)
#define fidelity_hash() (0UL)

#endif

#endif
