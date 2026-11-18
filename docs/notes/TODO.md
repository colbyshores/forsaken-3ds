# Open TODOs

## CargoDrone barrier stop position — defend2

**Level:** `defend2` (Remaster SP slot 5)

**Symptom:** The three `trackBarrier01/02/03.cob` BGOs on the
CargoDrone track stop the drone correctly, but the stop point is
INSIDE the barrier mesh — the drone overlaps the wall geometry
rather than stopping cleanly in front of it.

**Current code:** [enemies.c AutoMovementCrawl barrier loop](enemies.c)
uses `stop_r = drone.Radius + 60.0F` (~135u center-to-center). The
trackBarrier walls are wider than 60u along the drone's forward
axis, so the drone's front face crosses the barrier's near face
before stop_r kicks in.

**Why it works for everything else:** The trackBarriers have
`Pos=(0,0,0)` and rely on `.cob` children's authored world coords
for their actual location, so `bgo->Center` is correct. The 60u
buffer was tuned for those barriers but ends up too tight given the
wall thickness.

**Approach options:**

1. Project the barrier's AABB (`bgo->TopLeft` / `bgo->BottomRight`)
   onto the drone's forward direction:
   ```
   barrier_extent_fwd =
       |fwd.x| * half_extent_x +
       |fwd.y| * half_extent_y +
       |fwd.z| * half_extent_z
   stop_dist = drone_r + barrier_extent_fwd + ~30u buffer
   ```
   Caveat: bumps the stop range substantially.  Need to filter
   non-barrier OneOff_Anim BGOs (levers, knobs, doors) so they
   don't get caught — currently the loop sweeps everything and
   relies on the small 60u buffer to keep levers off-axis from
   matching.  Filter candidate: skip BGOs with
   `BumpTrigModPtr != NULL` (levers + knobs are bumpable; barriers
   aren't), or add an `IsCargoDroneBarrier` filename tag in
   `LoadBGObjectData` similar to the existing `IsCargoDroneLift`.

2. Reverse-engineer the KEX Forsaken Remastered binary via the
   Ghidra MCP server (`http://localhost:8080`) to find the
   authentic stop logic.  The KEX engine has its own
   barrier/CargoDrone handling — check `searchFunctions?query=Drone`
   or `searchFunctions?query=Barrier`.  Authentic stop offset
   would also tell us what the level designer intended.

3. Check `defs/n64Enemies.txt` (CargoDrone def) and any per-level
   data in the KPF for explicit stop positions.

**Status:** Deferred.  Drone DOES stop at each barrier, just overlaps
visually.  Returning here after other priorities.

---

## CargoDrone out-of-order final-switch routing — defend2

**Level:** `defend2` (Remaster SP slot 5)

**Symptom:** The 2nd-to-last switch is supposed to route the
CargoDrone to the last lift.  If the player flips it in order
(barrier lowered first via 3rd-to-last switch, then 2nd-to-last
switch), the drone correctly diverts to the final lift.

But if the player flips the 2nd-to-last switch FIRST and only THEN
lowers the barrier (3rd-to-last switch), the drone ignores the
diversion and continues looping its main path.

**Expected:** 2nd-to-last switch should latch a "go to last lift"
intent on the drone, applied whenever the drone next becomes free
to move — regardless of whether the barrier was already down at
flip time.

**Hypothesis:** The 2nd-to-last switch's trigger fires
`EnemySetTargetNode` (or `EnemySetNextTargetNode`) on the drone.
If the drone is currently blocked by the 3rd-to-last barrier,
its TNode/NextTNode state may already be locked to the looping
path, and the redirect event has no effect because:
- The redirect targets a node that's currently NOT a valid
  successor of the drone's TNode (drone behind closed barrier).
- OR the redirect is consumed/cleared before the barrier opens
  and re-enables the drone's movement.
- OR `aifollow.c`'s adjacency check in
  `PickNewNodeNow` (for `Enemy->Type == ENEMY_CargoDrone &&
  Enemy->NextTNode`) doesn't preserve the redirect across barrier
  release.

**Approach options:**

1. Trace the trigger sequence for the 2nd-to-last switch via
   `parse_trg.py` to see which TrigVars/TrigMods/Events fire and
   in what order.  Cross-reference against `triggers.c`'s
   `EVENT_EnemySetTargetNode` / `EVENT_EnemySetNextTargetNode`
   implementations to see whether the redirect persists across
   the barrier-still-closed state.

2. In `aifollow.c::PickNewNodeNow`, the CargoDrone hop-by-hop
   block already preserves `NextTNode` for the next adjacency
   check.  But if a barrier-induced full stop clears the
   redirect (or the redirect's target is unreachable until the
   barrier opens), the state is lost.

3. Reverse-engineer KEX Forsaken Remastered via Ghidra MCP — the
   original game must handle this correctly; check the switch's
   linked-trigger expansion for any "pending intent" pattern.

**Status:** Deferred alongside barrier stop-position work.
Player-recoverable (flip in order on retry).

---

## Resolved (kept for history)

### CargoDrone off-track on switch ~10-11 lift — defend2 — RESOLVED

Was: drone post-lift slightly off-track, walked the wrong direction
toward dead end, turned around to correct.

Apparently resolved by the pitch-lock fix in `enemies.c` (commit
603c710).  The pitched matrix was probably skewing the AI's
movement direction enough to push the drone laterally off the
intended track.  With pitch locked to 0, the drone walks straight
between nodes.

If this returns on any future testing, re-open this item.
