# Boss_Ramqan Research — Definitive Behavior Reference

Research conducted 2026-05-08 against Forsaken 64, Forsaken Remastered
gameplay sources, and KEX engine availability. Source citations at
end.

## What Boss_Ramqan actually does (canonical behavior)

**Locomotion** (already ported in `aijump.c`):
- Parametric arc leaps between AI nodes in his lair
- Floor-snap on landing
- Captured constants: `JumpT=75`, `arc multiplier=4`, `dt=1`,
  `floor-snap distance=1000`

**Firing** (NOT YET PORTED — currently missing entirely):

| Weapon | Behavior |
|---|---|
| Red lasers | Volley fire — multiple shots in quick succession |
| Scatter missiles | Spread pattern, **strips player weapons on hit** (signature mechanic) |
| Solaris missiles | Heavy single-shot |
| Homing missiles | Seek player position |
| Blaster cannon | Low-damage filler attack between heavy patterns |

Patterns alternate between leap and fire phases. He fires while
stationary on a node, then leaps to a new node, fires again. Not
fire-during-arc.

**Two-phase fight**:
- Phase 1: Boss in upper arena, leap+fire pattern
- Phase 2: Triggered at low health (~50% likely), drops into
  acid-filled lower room, continues fighting from there
- Acid is environmental damage to the player in phase 2

**Animation during arc**:
- Boss model is multi-component (`ramqan.cob`, 19 limbs per KPF
  extraction)
- During the leap arc, the multi-part rig should visibly animate —
  legs cycle, body bobs, joints move
- Currently does NOT animate visibly because `EnemyJumpUnderAiControl`
  skips `AutoMovementCrawl` (which drives per-frame COMP_OBJ child
  ticks) and only calls `AutoDisplay` (top-level matrix update only)

## What our 3DS port currently has

- ✅ Parametric arc leap locomotion
- ✅ Floor-snap on landing
- ✅ Per-frame group tracking for `MoveGroup`
- ✅ Two-pass node selection with dead-end fallback
- ✅ `SetCurAnimSeq(2)` at takeoff and `SetCurAnimSeq(4)` at landing
  fire correctly (top-level animation calls)
- ❌ **No firing logic at all** — boss never shoots anything
- ❌ **No visible animation during arc** — multi-part rig is frozen
- ❌ **No phase-2 / acid room behavior** (environmental hazard not
  driven by AI brain anyway)

## Implementation plan for v1.0 fidelity

### 1. Add firing to AI_JUMP

Add a stationary-fire phase between leaps. During the fire phase,
Boss_Ramqan sits on his current node, faces the player, and cycles
through:
- Red laser volley (3-5 shots, ~10 frames apart)
- Scatter missile spread (single launch, 3-5 missiles in cone)
- Brief pause
- Repeat until fire-phase duration elapses (~3 seconds)

Then call `JumpChooseNextNode` and start a new arc.

The 1998 engine has all the projectile types we need:
- `aifire.c::FirePosPnt` resolves the gun-fire offset on the boss
  model
- Existing enemy bullet types: scatter, solaris, homing, lasers
  already in `secbulls.c` / `bullets.c`
- We just need to invoke them from within `EnemyJumpUnderAiControl`
  during the stationary phase

### 2. Drive per-frame multi-part animation

When `JumpInAir == true`, also call the COMP_OBJ child-anim tick
that `AutoMovementCrawl` would have called. Either:
- (a) Call `AutoMovementCrawl` with a Speed=0 override so it does
  the anim tick but doesn't advance position
- (b) Identify the specific COMP_OBJ child-anim function inside
  `AutoMovementCrawl` and call it directly from
  `EnemyJumpUnderAiControl`

(b) is cleaner architecturally; (a) is faster to implement and
ships sooner.

### 3. Phase-2 acid-room behavior

Skip for v1.0. The acid room is an environmental hazard on the
level geometry, not AI behavior. The boss continuing to fight from
the lower room is a state transition we'd need a level-side
trigger for. Note as known limitation; revisit post-1.0.

## Approximation budget

Per the 80-85% fidelity bar:

- **Must nail**: leap behavior (done), at least one firing weapon
  type per fire phase, visible animation during arc
- **Should nail**: scatter missile signature attack (the
  weapon-strip mechanic is iconic for this fight)
- **Approximate**: exact volley counts, cooldown intervals, phase
  timings, multi-weapon rotation
- **Skip for v1.0**: phase 2 acid room, frame-perfect animation
  timing, exact projectile speeds

## Sources

- [Speedrun.com — Forsaken Remastered Ramqan's Lair](https://www.speedrun.com/forsaken__remastered/leaderboards?h=Ramqans_Lair-Easy)
- [Steam Community Guide — Forsaken Remastered](https://steamcommunity.com/sharedfiles/filedetails/?id=2292562310)
- [TV Tropes — Forsaken (1998)](https://tvtropes.org/pmwiki/pmwiki.php/VideoGame/Forsaken1998)
- [GameFAQs — Forsaken 64 walkthrough by Izaack](https://gamefaqs.gamespot.com/n64/197377-forsaken-64/faqs/45280)
- [Neoseeker — Forsaken 64 walkthrough](https://www.neoseeker.com/forsaken/faqs/27437-a.html)
- [PCGamingWiki — Forsaken Remastered](https://www.pcgamingwiki.com/wiki/Forsaken_Remastered)

KEX engine is closed-source (Samuel Villarreal / Nightdive
proprietary). `kexForsakenAIBrainJump` decomp not publicly
available; the implementation details above are inferred from
gameplay observation and are sufficient for 80-85% fidelity.

If frame-exact fidelity becomes a goal post-1.0, Ghidra on the
Forsaken Remastered binary would be the next step — but the
research above is sufficient for shipping.
