# Ralph-pi loop spec — Forsaken 3DS native-renderer performance work

This is the **iteration contract** for the unattended Ralph Wiggum loop driven by
the pi harness (`tools/ralph-pi.sh`). Each iteration is a fresh pi context; this
file plus the repo's own `AGENTS.md` are its entire memory. Anything not written
here or into the archive does not exist for the next iteration.

Branch: `experimental/native-renderer` (the loop works on the checked-out
branch; never switch branches).

---

## 1. Mission

Raise the frame rate of the Citro3D (native) renderer on the OG-profile proxy
toward 60 FPS by implementing the three remaining optimization items from the
native-renderer brief:

| Item | What it means | Notes |
|---|---|---|
| **3 — native render path** | Replay the PICA200 command stream directly instead of translating it through the GL-style path. | **SCOPE ALREADY PARTLY DONE**: `render_c3d.c` is a direct-citro3d renderer (`C3D_DrawArrays`, own vertex conversion, own shaders) and picaGL is **not linked** for it. The stale comment at `render_c3d.c:2773` ("GL-based via picaGL") and the TODO at `render_c3d.c:2774` ("replace with raw citro3d C3D_DrawArrays once visual output confirmed") are both wrong. Item 3 therefore means: **decide what is actually left of the "native render path" lever** (e.g. indexed draws via `C3D_DrawElements` instead of re-expanded triangle lists, no per-draw scratch rebuild, no per-draw state churn), fix the stale comments/TODO, and implement the chosen remainder with measured evidence. A banked statement that item 3 is already satisfied is a valid outcome **only if it cites the code that proves it** and states what was left undone. |
| **6 — pre-convert static vertices** | Convert level/static geometry to the GPU vertex format once (at load), instead of re-converting it every frame in `draw_render_object`. | `FSCreateVertexBuffer`/`FSLockVertexBuffer` (`render_c3d.c:1852`, `:1891`) hand out `LVERTEX`/`TLVERTEX`; the conversion to `gpu_vertex_t` happens per draw. Expected impact is low-to-moderate; measure, do not assume. |
| **7 — reduce entity work** | Cut per-frame per-entity CPU cost (entity iteration, visibility, model dispatch). | Candidate areas: `ModelDisp` (`models.c:1387`), `ProcessShips` (`ships.c:700`), the visibility bucket builder. Prior failed experiments (enemy/model/bullet culling, half-resolution, FRAMESKIP) are recorded in the archive — read them before re-proposing. |

**Order of work**: pick the bounded change with the best expected value per
iteration; do not attempt all three at once. Item 6 and item 7 are each one
bounded change. Item 3 needs a scoping decision first (see the table).

**Target**: 60 FPS on the OG-profile proxy. The proxy is `OG_PROFILE=1` (N3DS at
268 MHz, L2 off, OG heap/texture tiers) — it is **not** real Old 3DS hardware.

---

## 2. Repo memory (read these, in this order)

1. `AGENTS.md` — the performance and platform contract (R1–R11, T1/T2, P1–P6).
   It is binding: gates, exemptions grammar, and the do-not-reopen list.
2. `docs/validation/ralph-pi-log.md` — **the archive**. Read the LAST TWO
   `## iter NNN` entries (grep for the heading; never read the whole file).
3. `docs/validation/capture-protocol.md` — the capture/evidence protocol.
   are changing; do not read them whole.

---

## 3. Exact commands (the only sanctioned ones)

**Primary (unattended, safe): Emulator sweep.** Iterate on the emulator first for
fast feedback. Use this path for most changes.

```sh
make -f Makefile.3ds EDITION=remaster AUTOTEST_REMASTER=1 \
     AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 \
     PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d SLIM=1 -j"$(nproc)"
tools/perf/check.sh
EV="build_3ds/ralph-pi/evidence/perf-$(date -u +%Y%m%d-%H%M%S).log"
tools/perf/emu_sweep.sh --out "$EV"
python3 tools/perf/analyze_sweep.py "$EV" "$EV.analysis.txt"
```

**When ready → Byte alignment check.** Before testing on real hardware, verify
byte alignment issues don't exist. ARM11 is sensitive to alignment. Check:
- Struct layouts (especially vertex structs like LVERTEX/gpu_vertex_t)
- Unaligned accesses in hot paths
- The historical InterpFrames alignment fix pattern
- Ensure no new unaligned casts/memcpy patterns that could fault

Use tools to inspect, grep for suspicious patterns, and reason about alignment.
Document findings in the archive entry.

**Then test on real hardware.** Only after emulator iteration is stable AND
byte alignment checked, test on real hardware via the netload_stub pipeline.

```sh
# Build for hardware (may need different flags; check current working config)
make -f Makefile.3ds EDITION=remaster AUTOTEST_REMASTER=1 PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d -j"$(nproc)"
```

Then use the hardware pipeline: check ports, push via 3dslink, monitor via
`tools/perf/ralph_loop.sh` or `tools/perf/wave*_pipeline.sh`. Follow the
`3ds-autonomous-hardware-profiling-loop` runbook. If pushes fail or hardware
becomes unstable, fall back to emulator immediately.

**Fallback rule:** If hardware is unstable (transferrer failures, black screen,
stuck) or if alignment issues are suspected, drop back to emulator and
investigate there. The loop must utilize the emulator whenever it sees fit.

---

## 4. Evidence rules (do not skip these)

- **An emulator number is never a hardware number.** Mandarine runs the guest
  CPU at host speed, has ~5 MB linear heap (hardware: 22–24 MB), lenient
  clamping and different f24 precision. Emulator timings are **directional**
  evidence for ranking and for spotting regressions inside this loop. They do
  **not** clear any `AGENTS.md` P6 hardware claim
  (`docs/validation/capture-protocol.md` §6). Say so in every entry that quotes
  a number.
- **A/B, not one-shot.** A claim about a change needs a baseline artifact and a
  candidate artifact from **matched** runs (same flags, same level, same
  emulator, same display). Compare `stage=` totals per frame via
  `analyze_sweep.py`. A single run is a smoke test, not a measurement.
- **Freshness**: `emu_sweep.sh` refuses to report a stale artifact (md5
  unchanged) and removes the previous artifact before each run. If it exits 3,
  the run produced nothing — that is a FAILURE to diagnose, not a null result.
- **Scope of a claim**: if the artifact does not cover the level/feature you
  changed, the entry must say the claim is UNVERIFIED.
- **Never invent a measurement.** If you did not run it, it is not a result.

---

## 5. Archive format

Append to `docs/validation/ralph-pi-log.md`. One entry per iteration, heading
`## iter NNN — <short slug>` (NNN = next number, zero-padded to 3). Entry
template:

```markdown
## iter 007 — <slug>

- Item: 3 | 6 | 7 | <other>
- Change: <one line: what changed, file:line>
- Risk: low | medium | high          <-- REQUIRED, drives the review lane
- Build: <exact command> -> exit=<n>
- Gates: tools/perf/check.sh -> exit=<n>
- Evidence: <artifact paths> (baseline <path>, candidate <path>)
- Result: <the measured delta and what it means; emulator-scoped>
- Unverified: <anything you could not check>
- Next: <the single next bounded action>
```

Rules:

- Candidates, not verdicts: the independent reviewer (`router/verify`) appends
  `VERDICT(verify): ...` lines. Do not write verdicts yourself.
- The `Risk:` line is what the loop uses to decide whether to invoke the
  reviewer. Classify honestly:
  - **low** — small/local, no interface/architecture/state change, no
    concurrency/memory/resource-lifetime change, gates pass.
  - **medium** — bounded but touches behavior, data layout, or a hot path in a
    way that could regress gameplay.
  - **high** — architecture/interfaces/state transitions, memory safety,
    concurrency, resource lifetime, performance-critical code, or a fix whose
    root cause is still uncertain.
- Never rewrite a previous entry. Corrections are appended as a new entry.

---

## 6. Review policy (binding)

The builder implements, the verification harness proves, an independent reviewer
challenges. The reviewer is a **different model family** (`router/verify`, pinned
machine-wide to local qwen then muse) and is **read-only** — it appends one
verdict line and changes nothing else.

- **low** risk → no independent review of that entry.
- **medium** risk → reviewed in batches (the loop reviews at least every 5
  completed iterations, and always before the mission is declared complete).
- **high** risk → reviewed immediately; the entry must say so in its first
  line: `RISK-ALERT: high` so the loop reviews it without waiting for the
  batch.

Review findings are advisory. The builder assesses them and implements focused
fixes; a rejected finding is recorded with a one-line technical reason. Never
weaken or delete a gate to make something pass.

---

## 7. Safety

- **Do not commit, push, publish or deploy.** Leave the change in the working
  tree; the operator commits. (The loop's own files under `build_3ds/` are
  gitignored.)
- **Do not modify**: `AGENTS.md`, `tools/perf/contracts/*` (exemptions,
  baselines, corpus) unless the mission is explicitly about them, and never
  `tools/perf/check.sh` to make it pass.
- **Do not weaken acceptance criteria** to converge the loop.
- **Do not run two emulator sweeps at once** — `emu_sweep.sh` holds a
  per-display lock; respect it.
- **Do not touch the hardware console** (`192.168.4.52`, netload_stub, 3dslink).
  Hardware validation is a human step and the console is currently unreliable.
- **Search scope**: this repo only. `/opt/devkitpro/` only for devkitPro
  header/tool questions. Never `/`, `$HOME`, `/mnt/nas`, or sibling repos.
- **Leave the tree buildable**: if an iteration cannot finish its change, revert
  it before ending the iteration. The next iteration starts from a clean build.
- Do not overwrite unrelated user changes in the working tree (`git status`
  before you start; leave anything you did not create alone).

---

## 8. Completion criterion (derived; the loop exits on `<promise>DONE</promise>`)

The mission is complete when ALL of the following are true and banked in the
archive:

1. **Item 3** has a banked verdict: either the remaining native-path work is
   implemented with matched emulator A/B evidence, or the entry states (with
   code citations) that the item was already satisfied and what, if anything,
   was left undone — and the stale comments/TODO in `render_c3d.c` are fixed.
2. **Item 6** and **item 7** each have a banked verdict: implemented with
   matched emulator A/B evidence, OR a banked explanation of why the item was
   measured and rejected (a null result is a valid result — record it, do not
   re-run until a win appears).
3. `tools/perf/check.sh` exits 0 at the final tree state.
4. A banked frame-budget assessment: the emulator-measured per-frame CPU work
   against the 16.67 ms (60 FPS) budget, explicitly labelled **not** a hardware
   pass, with hardware validation listed as the remaining human step.
5. No unresolved high-severity reviewer finding, and no unexplained regression.

The loop emits `<promise>DONE</promise>` **only** when all five hold. A DONE is
a claim, not a fact: the operator re-scores it.

**WATCH-THE-CLOCK**: the loop kills an iteration at its wall limit. If the
elapsed time in your footer passes `wall − 30 min`, STOP investigating and BANK
NOW under the next `## iter NNN` heading. A banked partial beats a killed full.
