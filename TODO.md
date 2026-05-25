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

## CargoDrone slight off-track / overshoot on switch ~10-11 lift

**Level:** `defend2`

**Symptom:** On one of the lifts (user identified as "switch 10 or
11", which maps to either lift 1 / BGO[27] in grp 19 or one of the
knob-triggered lifts), the drone after the lift is slightly off the
track, moves in the direction of the track, reaches the end of
something, and turns around to correct itself.

**Hypothesis:** After lift end, drone is at `(wait_node XZ,
upper_node Y)`.  TNode is advanced past `upper_node` to next-next
in the next AI frame.  The straight line from drone's position to
next-next can cross corridor geometry sideways if `upper_node` XZ
is significantly offset from `wait_node` XZ.

**Tried:** Always-teleport drone to `upper_node` SolidPos at lift
end (for XZ-offset > 50u).  Worked geometrically but visually
jumped the drone backward off the lift platform — wrong.

**Approach options for retry:**

1. Reverse-engineer the KEX behavior — look for `kexCargoDrone` or
   similar in Ghidra.  The original game must have handled
   single-segment-lift-with-offset-upper somehow.

2. Smaller XZ correction: instead of full teleport to upper, nudge
   drone toward upper's XZ by some fraction each frame after lift
   end until the drone is on-axis.

3. Replace the AI's straight-line path-following with a "must visit
   upper_node" intermediate step after lift end.

**Status:** Deferred.  Behavior is occasional and recoverable;
addressing barriers first.
