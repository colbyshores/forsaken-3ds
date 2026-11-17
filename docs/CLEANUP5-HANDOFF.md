# Cleanup 5 handoff spec — C-with-classes conversion, module by module

Written 2026-11-16 for the next implementer (human or local model).
Goal: convert the Forsaken engine from C to "C with classes" per
AGENTS.md §9, in the t2k/mdk-sdl house style, **without changing
any behaviour**. Read AGENTS.md in full before touching code —
especially §9 (language model), §3/§4 (perf rules — the gates scan
`.cpp` too), and §5 (exemptions). This spec is the operational
playbook; AGENTS.md is the law where they differ.

## 0. Status update 2026-11-17 — read this first

- **Cleanup 5 (phase 1, C → `.cpp` translation) is complete.** Every
  in-scope module is a `.cpp`; what stays C is recorded with its reason.
  §1/§7 below are the historical plan, not the current state.
- **The ledger is `docs/CPP_LEDGER.tsv`** — one row per translation unit:
  path, language, phase-1 commit, phase-2 status, host test, fidelity A/B
  status, and why a file stays C. Update the row in the same commit that
  changes it; the git log is the evidence, the ledger is the index.
- **Source layout changed** (move-only, object code proven identical): the
  flat root is now `forsaken_core/src/{game,ai,world,data,rendering,audio,
  net,scripting,input,ui,math,util,diag}`, `forsaken_3ds/`, `forsaken_pc/`
  (`"game/ships.h"`). The §4 commands still work from the repo root; the
  §5 gate re-key table no longer applies to moves (gates key by basename).
- **Phase 2 (real classes) conventions**, recommended 2026-11-17 and open to
  owner override: t2k layout and header conventions (`#pragma once` on new
  or rewritten headers; header comments state *why*, units, ownership;
  root-relative includes), **Forsaken's own identifiers kept** (no renaming
  to snake_case, no other project's names or prefixes), plain structs +
  free functions remain the default (they already are: `Ships[]`,
  `Enemies[]`), and member functions only where they earn it — value types
  (QUAT, done in `872d887`) and modules owning private state (the
  `Hunk`/`StereoMemo` pattern). No namespace around anything a C TU
  calls; if a namespace is introduced for C++-only internals it is
  `forsaken`. Replace local `extern "C"` declarations in `.cpp` files with
  the owning header (39 files still carry them; `oct2.cpp` 26,
  `title.cpp` 30). Each phase-2 module still needs its fidelity A/B.

### Phase 2 progress (2026-11-17, branch `experimental/cpp-phase2`)

Done, each commit proven with `tools/verify_identical.sh` (clean builds, all
configurations; `tools/verify_o0.sh` where `-O2` scheduling differs):
- Every shared global/function declared once, in its owner's header, from
  the definition (~2,000 private `extern` copies gone); `game/constants.h`.
- Every translation unit is C++; `extern "C"` only at real C boundaries
  (AGENTS.md §9). Lua only via `scripting/lua_api.h`.
- Dead code: `#if 0` blocks, commented-out code (`//` runs and `/* */`
  blocks, incl. every D3D execute-buffer remnant), conditionals on 28
  obsolete macros, 61 prototypes for functions that do not exist, and
  300 functions/globals nothing uses (~11,000 lines in all).
- The tree is the game players play (owner, 2026-11-17): the
  forsaken-harness instrumentation (`diag/`, 160 hook sites, ATM/fidelity/
  framedump/perf/autotest knobs and tools), all tracing (`trace`,
  `boot_log` -- which was NOT gated and fsync'd to SD on every call in the
  shipping build), and the test scaffolding (3DS `DEBUG=1` god mode, warps,
  stereo debug overlay + menu toggle, `stereo_test_slider`, `SLIM`, the
  `MODEL_VIS_CACHE` experiment) are gone. AGENTS.md no longer requires a
  harness A/B; proof is `verify_identical.sh` + an owner play test for real
  behaviour changes.
- `tools/verify_identical.sh` proves 3DS remaster/original and PC `host`
  (default `DEBUG=1`, the only build compiling `DEBUG_ON` code) and
  `host-release` (`DEBUG=0`).
- Build: header dependencies are tracked on 3DS again (they never were).

Bugs found (separate, labelled commits): `primary.cpp` indexed
`ModelHeaders` as an array (1c75966); PC joystick names (5d6474b); the C++
conversion briefly lost the libctru heap override (088b1f0, now guarded).

**On-device play test:** 2026-11-17, owner, build 7b095b6 (remaster .3dsx
via ftpd): title + the subway level play well, no regressions seen -- covers
46d152e (platform TUs / render_c3d as C++), da7b53a (no tracing) and
8260103 in general play. Emulator (Mandarine r1.2.1) and PC Vulkan
(lavapipe) title screens match a 09eeb31 baseline. Still worth a targeted
check: 1c75966 (spot-FX bullets from models, e.g. the reactor core) and
the 3DS Visuals menu (stereo debug row removed).

**Data fix, not in git (romfs/ is gitignored):** the volcano lava floor drew
another image on New 3DS because `romfs/hd_textures/levels/vol2/textures/
thermd.t3x` (rewritten 2026-10-25 00:41) declared 1,024 of its 262,144 data
bytes (size field `00 04 00` instead of `00 00 04` at offset 0x12). Patched
in place 2026-11-17 (payload untouched); `tools/texture/t3x_check.py
romfs/hd_textures romfs/hd_textures_lomem` now passes all 816 files. If
romfs/ is ever regenerated, rerun the checker. Owner confirmed on hardware.

**Owner decisions pending:**
1. Optional features under never-defined macros — keep or delete:
   `DEMO_SUPPORT`, `LUA_BOT` (botai_*, lua_bot: compiled empty everywhere),
   `PLAYER_SPEECH_TAUNTS`, `COOPERATIVE`, `SCROLLING_MESSAGES`,
   `SINT_PEACEFROG_CHEAT`, `POLYGONAL_COLLISIONS`, `ACTUAL_TRANS`,
   `NEW_LIGHTING`, `MINIUPNP`, `BREAKPAD`, `NET_ENET`, `TEXTURE_SDL`/`DEVIL`,
   `SOUND_DSOUND`, `RENDER_DISABLED`, `INPUT_DISABLED`, `EXTERNAL_DEMO`,
   `HOST_CHOOSES_DEMO_NAME`, `REMOTE_CAMERA_ENABLED`, `TEAM_NUMBERS`.
   Behaviour, not just code: `FINAL_RELEASE` (undefined, so non-original
   levels follow the original-campaign completion path -- was that the
   1998 release's behaviour?). Debug switches kept as-is:
   `DEBUG_TEXT_MESSAGES`, `LIGHT_EVERYTHING`.
2. Windows-only files never built: `render_d3d.cpp`, `breakpad.cpp`,
   `sound_dsound.cpp`, `input_dinput.cpp`, `texture_devil.cpp`.
3. Multiplayer: the init packet carries `BYTE[16][16]` kill stats into
   `int[17][17]` arrays by raw copy (FIXME in networking.cpp).
4. Gates: `title.c` exemptions (E-TITLE-1, ALLOWED-COLD-TITLE) have been
   dead since Cleanup 5 — re-point (loosens the gate) or delete; ~50
   baseline entries match nothing (more now that dead code is gone) — prune?
5. Lua binding modules (`lua_bullets/collision/controls/enemies/object/
   ships/vecmat/weapons`) are never registered: `assign_loaders` registers
   nothing, so no script can load them. Delete them, or wire them up?
6. PC `Makefile` hard-codes `DEBUG=1` (`-O0 -g`, `DEBUG_ON`) and
   `PROFILE=1` (gprof `-pg`, writes `gmon.out`). Keep, or default to an
   optimised build without profiling?
7. Off-binary dev tooling that stays unless you say otherwise: host unit
   tests (`tests/`), the static perf/contract gates (`tools/perf/`, run by
   `build_matrix.sh`), `verify_identical.sh`/`verify_o0.sh`.
8. Support channel: `sdmc:/forsaken_boot.log` (tier/texture-pack lines

Next (no decision needed): data written but never read (e.g.
`BspZNames[]`), empty `{ }` blocks and bare blocks left by earlier passes,
`#pragma once` + header comments, the 2,566 warnings `-isystem` was hiding, `constexpr`/`enum class`/`std::array` sweep, then classes for
state-owning modules (A/B batch).

**Last, after the conversion is complete (owner request 2026-11-17):** fully
refactor the Forsaken Obsidian docs (`port-docs` vault,
`10-projects/forsaken/` and the patterns promoted from it) to match the new
source layout, file names (`.cpp`, `forsaken_core/forsaken_3ds/forsaken_pc`
trees), seam headers and class structure. Most notes still cite
(it still documents tracing, autotest, `DEBUG=1`, `SLIM`, the test-harness
branch). The cross-project lesson so far is in
`30-architecture/patterns/c-to-cpp-private-externs-hide-type-drift.md`.

---

## 1. Where things stand

Branches (origin = `colbyshores/forsaken-3ds-private`, private):

| Branch | Tip | Meaning |
|---|---|---|
| `master` | `5f25279` | Unification + Cleanups 1–4, merged and hardware-validated on OG 3DS (see §8). |
| `experimental/cpp` | (this branch) | Cleanup 5 work. Base is `035dddf` (= master's content) + pilot. |
| `unify/vulkan-etc2-music` | `035dddf` | Merge source record. Safe to ignore. |
| `experimental/frame-interp`, frame-interp worktrees | deleted | Do not resurrect. |

Converted so far (each its own commit on `experimental/cpp`, tip
at writing: `d9d4016`):

`dsp_adpcm`, `hunk_alloc` (pilot, `4f19b6a`), `quat` (`bf5252c`),
`sphere` (`26191fa`), `stereomemo` (`3bd8416`), `xmem`, `timer`
(`12e46c1`), `music_table` (`87549bd`), `new3d` (`a584770`),
`util`, `file` (`6836f6b`), `texture_png` (`4bf7fc9`), `camera`
(`c7aa8fe`), `lines` (`a55c06e`). Headers already carrying
`extern "C"` guards: `dsp_adpcm.h`, `hunk_alloc.h`, `quat.h`,
`new3d.h`, `sphere.h`, `stereomemo.h`, `xmem.h`, `timer.h`,
`platform.h`, `music.h`, `music_table.h`, `texture.h`, `file.h`,
`util.h` (pre-existing), `camera.h`, `lines.h`, `main_3ds.h`.

Verified state at handoff: both builds link, **ALL GATES PASS**,
host suite passes (`test_modules`, `test_quat`, `test_sphere`,
`test_new3d`, `test_stereomemo`), Linux title scene renders
textured under the Vulkan backend, master passed its OG-3DS
hardware batch (bench avg ~74.3 ms stereo on ramqan via harness,
fidelity FIDC checkpoints byte-identical, framedump FNV-1a match).

## 2. The conversion pattern (per module, exactly)

The pattern is **translation, not rewrite** (AGENTS.md §9):

1. **Rename**: `git rm -q foo.c`, then `git show HEAD:foo.c > foo.cpp`
   (or `git show HEAD~:foo.c` if you already removed it). Keep every
   line of math/comparison/literal identical. Never retype a float
   literal, never reorder a comparison, never "clean up" `FMOD`/
   `FDIV`/`fast_sinf` macros.
2. **Guard the header**: wrap `foo.h`'s declarations in
   `#ifdef __cplusplus / extern "C" { / #endif` … `#ifdef
   __cplusplus / } / #endif`. Put the open *after* `#include`s
   (never around system headers), the close *before* the include
   guard's final `#endif`. If the header only has types/macros it
   may not need it — but any function declaration does.
3. **Class-ify where there is state**: if the module owns state
   (tables, counters, buffers), move it into one `struct` in an
   anonymous namespace in the `.cpp`, with methods; the public
   `foo_*` C functions forward to a single static instance. See
   `hunk_alloc.cpp` (`Hunk`), `stereomemo.cpp` (`StereoMemo`),
   `xmem.cpp` (`XMemTracker`), `dsp_adpcm.cpp` (`DspDecoder`) for
   the shape. If the module is pure functions (`sphere`, `quat`,
   `new3d`), a straight translation is correct — do not invent a
   class. Header comments: document layout/conventions/invariants
   (t2k style: say *why*, units, and ownership), e.g. `quat.h`.
4. **Fix only C++-forced issues**, each is a known trap (§3).
   Anything that smells like a *behaviour* fix goes in its own
   commit with its own justification — never smuggled into a
   conversion commit.
5. **Host test**: if `tests/` has a test for the module, point it
   at the `.cpp` and run it. If the module is host-testable and
   has no test, add a small one (`tests/test_<mod>.cpp`, wired
   into `tests/Makefile` like the others) pinning *observed*
   behaviour — including quirks (see `test_sphere.cpp` on
   `intersect()`'s from-inside root). Tests pin behaviour; they
   do not define it. If your test's expectation and the code
   disagree, the code is probably right and your expectation is
   wrong (this happened twice: `test_modules` predictor math,
   `test_quat` matrix convention).
6. **Verify** (§4), then **commit one module per commit** (§6).
7. **Re-key the perf gates** if the module is filename-keyed (§5)
   — in the same commit if it's only the rename following the
   file, or a follow-up `perf:` commit. Never leave gates red.

## 3. Trap catalog (every one of these has bitten)

- **Name mangling**: a converted `.cpp` calling an unguarded C
  header gets mangled references at link. Fix by guarding that
  header (§2.2), not by wrapping call sites. Order of pain:
  `new3d.h`, `xmem.h`, `platform.h`, `music.h`, `texture.h`,
  `camera.h`, `lines.h` are done; expect the same for every
  module header from here on (`main.h` has almost no function
  declarations — per-module headers are the seam).
- **Tentative definitions in headers**: C links them via
  `-fcommon`; C++ rejects duplicates at link
  (`networking.h`: `host_address`, `PlayerReady`, tracker vars;
  `visi.h`: `Num_IndirectVisible`). Fix: `extern` in the header,
  one definition in the owning `.c`. Grep the header for
  `^<type> <name>;` / `^<type> <name>[...]` definitions first.
- **`void*` results**: `p = malloc/calloc(...)` needs a cast in
  C++ (the xmem macros expand to `X_malloc`, same issue). Cast at
  the assignment, exact target type.
- **`void*` arithmetic**: e.g. `memset(Pnt + n, ...)` — cast
  through `char*`. Identical semantics.
- **String APIs at `u_int8_t*` boundaries**: explicit `(char*)` /
  `(const char*)` casts (see `util.cpp` `GetFilename`).
- **`sizeof(pointer)` bugs**: this tree has them
  (`memset(p, 0, sizeof(Pnt))`). `X_malloc`'s is fixed; one more
  was in `destroy_image` (`texture_png.cpp`, fixed). If you find
  another, fix it in a *separate* commit saying so.
- **Implicit declarations truncate pointers on 64-bit** (hit
  three times: `glGetStringi`, `file_open`, …). Before first
  running any build with a new TU, check the build log for
  "implicit declaration" and fix the include.
- **`register` keyword**: still accepted by gnu++17; leave it.
- **The trig LUT**: `MakeQuat` & friends read `fast_sin_table`;
  host tests must call `trig_table_init()` first or everything
  reads zeros (looks like a conversion bug; it isn't).
- **Static-init order**: converted structs are POD/value types
  with no constructors doing work. Keep it that way; a global
  with a real constructor is a §9 review item.
- **No user-defined operators on float-bearing types** (phase 1),
  no virtuals/RTTI/exceptions/templates/`std::function` — the
  gate `audit_c_with_classes` enforces the banned list and
  *will* fail the build's `check.sh`.
- **`tests/Makefile` is fragile**: anchor edits on exact rule
  text and re-read the file after editing (two corruptions
  happened from blind `sed` replaces). After any edit there,
  run every test target you touched.

## 4. Verification loop (per module, in order)

From repo root (`/mnt/nas/Development/forsaken`):

```sh
# Linux build in a scratch copy (never build in-tree for the PC target;
# .o files pollute the checkout)
rm -rf /tmp/cxxN && mkdir -p /tmp/cxxN
tar --exclude=.git --exclude=build_3ds --exclude=.romfs_staging \
    --exclude=picaGL --exclude=.romfs_staging_cia -cf - . \
  | tar -xf - -C /tmp/cxxN
cd /tmp/cxxN && make 2>&1 | grep -E "error:|undefined" | head
ls projectx   # must exist

# 3DS build (in main tree is fine; it is out-of-tree via BUILD=)
make -f Makefile.3ds BUILD=/tmp/cxx-bN EDITION=remaster
ls forsaken3ds.3dsx && rm -f forsaken3ds.3dsx forsaken3ds.elf \
  forsaken3ds.smdh forsaken3ds.map && rm -rf .romfs_staging

# gates (from repo root)
bash tools/perf/check.sh        # must print ALL GATES PASS

# host tests
cd tests && make test_modules test_quat test_sphere test_new3d test_stereomemo
```

Note: `grep -cE "error:"` counts `math_error.c` filename hits —
always also check the artifact exists. Emulator smoke
(Mandarine r1.2.1 only — **never r1.3.0**, it segfaults on level
load; KB: `~/Documents/port-docs/50-tooling/mandarine-emulator-
location-and-version-pinning.md`): run the 3DS build on your own
Xvfb display (tenants run Perfect Dark concurrently — never kill
a PID you didn't create), confirm it reaches the title menu.

## 5. Perf-gate re-keying (renames break filename keys)

The gates key findings by filename. Every rename must re-key:

| File | What to update |
|---|---|
| `tools/perf/contracts/hot_findings_baseline.txt` | `RULE\|old.c\|` → `RULE\|old.cpp\|` for the converted file (all rules: R2/R4/R4-D/R7/R9/R11 lines). |
| `tools/perf/contracts/r11_candidates_baseline.txt` | Same if present there. |
| `tools/perf/contracts/hotpath_corpus.json` | `"file": "old.c"` entries. |
| `tools/perf/gates_vfp.py` | Standing exemptions keyed by file (e.g. E-LUT-3 `trig_table_init` → `new3d.cpp`, done). |
| `tools/perf/audit_arm11_math.py` | `ALLOWED_FILES` dict (e.g. `"new3d.cpp": "ALLOWED-LUT-CONSTRUCTION"`, done). |
| `tools/perf/audit_memo_stamps.py` | File table + `SCAN_FILES` (e.g. `lines.cpp`, done). |
| `tools/perf/contracts/vfp_hotpath.md` | Doc references ( cosmetic; keep accurate). |

Beware double-renames from overlapping seds (`.cpppp` happened).
After re-keying, `bash tools/perf/check.sh` must be ALL PASS.
Baseline ≠ exemption (AGENTS.md §5): re-keying follows a rename
only; never add a *new* baseline entry to silence a finding —
that needs the exemption grammar and a review, not a baseline.

## 6. Commit conventions

One module per commit. Subjects used so far:
`Cleanup 5: convert <mod>.c to C-with-classes (<mod>.cpp)`.
Gate re-keys and test fixes are separate `perf:`/`tests:`
commits. Message body states: translation-only or what changed,
what was verified (exact builds/tests/gates), and the standing
line **"Hardware fidelity A/B deferred to this branch's merge."**
Never claim a check passed that you did not watch pass in the
same session (this bit twice; both times the correction went in
a follow-up commit naming the earlier claim). Do not push
without the owner's ask; do not amend/rebase published history.

## 7. Remaining modules (suggested order)

Leaves first (small, host-testable), then loaders, then the
big gameplay TUs last. Sizes are lines of `.c` at handoff.

**Tier 1 — leaves:** `singleplayer.c` (142), `math_error.c`
(179), `demo.c` (92), `lua_config.c`/`lua_controls.c` (~150,
guard the lua headers they need), `aispline.c` (198, spline —
double math, R4-pinned: translate only), `aipingoff.c`,
`airetreat.c`, `aiidle.c`, `aiscan.c`, `aimovet.c` (AI leaves;
their headers will need guards).

**Tier 2 — subsystems:** `tload.c`, `mload.c`, `mxload.c`,
`mxaload.c` (model/texture loaders — alignment-sensitive,
`memcpy_unaligned` discipline from AGENTS.md §6 applies, do not
"fix" alignment code), `polys.c`, `2dpolys.c`, `screenpolys.c`
(hot path; the memo-stamp gate watches these — keep stamps),
`models.c`, `compobjects.c`, `transexe.c`, `water.c`,
`bgobjects.c`, `enemies.c`, `ships.c`, `pickups.c`, `primary.c`,
`secondary.c`, `spotfx.c`, `rtlight.c`, `lights.c`, `node.c`,
`visi.c`, `bsp.c`, `collision.c`, `checkenemy.c`,
`trigarea.c`, `triggers.c`, `extforce.c`, `goal.c`, `stats.c`,
`restart.c`, `loadsave.c`, `config.c`, `controls.c`,
`skin.c`, `text.c`, `sfx`/`sound_*` (platform audio stays C++
-agnostic longest — owner's call), `networking.c`, `net_enet_2.c`,
`multiplayer.c`, `botai_*`, `aimain.c`, `aim_assist.c`.

**Tier 3 — the big three, last and opportunistically:**
`oct2.c` (~190 KB), `title.c` (~500 KB), `models.c` overlaps
tier 2. Per AGENTS.md §9 these convert only with a per-module
fidelity A/B on hardware (§8). If in doubt, leave them in C —
a converted leaf set plus C core is a legitimate end state;
the owner decides when the big ones move.

**Do not convert:** `main_3ds.c`, `input_3ds.c`, `sound_3ds.c`,
`controls_3ds.c`, `stubs_3ds.c`, `music_3ds.c` (platform TUs —
owner's call, and `music_3ds.c`'s ndsp code has no C++ need),
`render_c3d.c`, `render_vulkan.c` (backends; `render_vulkan.c`
may convert late for `VkHandle` typing if ever wanted — not
needed), anything in `picaGL/`, `probe/`, `tools/`,
`forsaken-harness/` (separate repo, §9 there).

## 8. Hardware validation (owner-gated, do not improvise)

The appliance (OG 3DS `old3ds-lan167`, 192.168.4.167) is shared
and was busy for most of this program; the owner says when it is
free. When it is:

- Read `/mnt/nas/Development/netload_stub/docs/atm-autonomous-
  agent-runbook.md` first; it outranks everything else. Never
  `atm run`/`atm launch` a `.3dsx` (3dslink push only), never
  ask for physical actions, prove reboots by `boot_count`
  increase + `recovery_ready=yes`, clean staged payloads with
  `atm del` + `atm artifacts`.
- Bench/fidelity machinery lives in the **harness repo**
  (`/mnt/nas/Development/forsaken-harness`, `ATM_PROFILING`/
  `FRAMEDUMP`/`FIDELITY_DUMP` builds), not in this tree.
- Per-module fidelity A/B (AGENTS.md §9 merge gate): build the
  module as C and as C++, sweep both on hardware with
  `FIDELITY_DUMP=1`, checksums in `sdmc:/forsaken_fidelity.txt`
  must be byte-identical. Batch conversions into groups (e.g.
  all of tier 1) rather than one console run per module.
- Master's batch (2026-11-16, agent-run) is the template:
  release smoke + harness bench (ramqan stereo, 240 frames)
  + FIDC fidelity + framedump FNV-1a; evidence in
  `perf/hwval-20261116-merge5/NOTES.md`. Note its caveats:
  `perf/BLOCKED.md` is stale (operator-only, do not clear),
  and `atm del` cannot prove absence for 3dslink-pushed files.

## 9. Wider context (do not lose)

- The tree also carries: perf contract + gates (`AGENTS.md`,
  `tools/perf/`), the audit findings in
  `~/Documents/port-docs/10-projects/forsaken/findings/`
  (X_malloc overrun, ETC1 storage conventions, frame-interp
  dropped by owner decision — do not reimplement it).
- `Data/` is gitignored on purpose (assets are local and
  regenerable: PNG masters via `tools/texture/etc2enc.c` at PC
  build time; recovered masters via `tools/texture/t3x2png.py`).
  Never commit game data.
- A pristine public `forsaken-3ds` repo is planned from master
  (owner trigger required; asset/license inventory first —
  `LICENSE` has an abandonware/GPL-mixed notice). Do not create
  or push it without the owner saying so.
- Real-GPU Vulkan numbers are still unmeasured (llvmpipe only;
  Xvfb lacks DRI3). Emulator numbers are never OG-3DS evidence.

## 10. Definition of done for Cleanup 5

Every in-scope module converted (or explicitly left in C with a
reason recorded), one commit each, both builds + gates + host
tests green at the branch tip, per-module fidelity A/B passed
on hardware in batches, then merged to `master` in a worktree
with the same proof standard as merges 1–5, and
`experimental/cpp` deleted.
