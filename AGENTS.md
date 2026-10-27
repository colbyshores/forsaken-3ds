# Forsaken 3DS — performance and platform contract

Revision: 2026-10-25. Shape adopted from T2K's `AGENTS.md` (rev 2026-10-19);
the rules are re-derived against this tree, not copied blind. Full rule text,
exemption grammar, standing exemptions, and the report schema:
`tools/perf/contracts/vfp_hotpath.md`. Hot-function corpus:
`tools/perf/contracts/hotpath_corpus.json`. Gates:

```
python3 tools/perf/gates_vfp.py        # R1–R11 hot-path audit + build flags
python3 tools/perf/audit_arm11_math.py # trig-LUT coverage + double residue
python3 tools/perf/audit_r11_idiv.py   # runtime integer div/mod screen
tools/perf/check.sh                    # all three
```

**Project policy and hardware facts are different.** R2/R4/R6 and the other
conservative rules are deliberate constraints for this engine. They are not
proof that every prohibited construct is always slow on every 3DS workload.

The original 3DS is the performance target: ARM1176JZF-S at 268 MHz, VFP11
(VFPv2), 16 KB L1I + 16 KB L1D, no L2. This build enables the New 3DS CPU
boost when present (`main_3ds.c:312-316`), so **every perf claim must state
the console, clock, and cache settings it was measured on. A New-clocked run
is not an OG pass.** Record stereoscopic mode too: stereo doubles the CPU
scene traversal in this engine (§6).

The engine ticks at 71 Hz (`CalculateFramelag`, `oct2.c:6480`;
`framelag = real_framelag * 71.0F`). Frame budgets in this file are
milliseconds per 71 Hz frame.

---

## 1. What the ARM11 actually taxes

Floating-point arithmetic is supported in hardware (VFPv2: `vadd`/`vmul`/
`vdiv`/`vsqrt`, basic `f64` too). Costs come from four places:

1. **Coprocessor boundary crossings.** ARM↔VFP transfers (`vmov`), integer↔
   float conversions (`vcvt`), and float comparisons read by ARM branches
   (`vcmp` + `vmrs`) are each their own operation — ARM11 MPCore TRM §19.11
   Table 19-16 lists 1-cycle throughput with 2–8 cycle latency. Throughput
   and latency are different numbers; dependency chains serialize. Do not
   add table entries and call the sum a measured loop cost — inspect
   generated instructions and profile the actual loop.
2. **libm calls that cross the boundary per iteration.** This is Forsaken's
   live debt: VFPv2 has **no float-result floor instruction**, so `floorf`
   is a real `bl` into newlib with `vmrs`/`vmsr` flag transfers and a spill
   frame (`new3d.h:39-61`). The `FMOD`/`FDIV` macros (`new3d.h:105-106`)
   expand to `floorf` inside per-entity loops (`models.c`, `secondary.c`,
   `2dpolys.c`, `bgobjects.c`, `spotfx.c`, `rtlight.c`, `pickups.c`), and
   `mxaload.c:1195-1199` runs four per vertex in `InterpFrames`.
3. **Subnormal handling.** FPSCR FZ/DN are **not set anywhere in this tree**
   (audit 2026-10-25). R1 fixes that as a correctness pin, not a speedup
   claim.
4. **Register pressure and footprint.** 32 single-precision registers;
   doubles consume two; spills are traffic on a 16 KB L1D with no L2 (OG).
   Count generated loads/stores, not source variables.

And one non-FP fact: **ARMv6 has no integer divide instruction.** Runtime
integer `/` or `%` lowers to `__aeabi_idiv`/`__aeabi_idivmod` — tens of
cycles plus a call barrier that spills live registers. VFPv2 *does* have
hardware `vdiv`/`vsqrt`; that is not permission to replace exact integer
math with float math.

This contract does **not** prohibit floating-point math or mixing integer
and float values in the same function.

---

## 2. HOT PATH

A function or loop body is HOT PATH if it:

- runs per-frame at ≥ 1× frame count AND is in the top 15 of a captured
  profile, OR
- runs ≥ 256 times per frame regardless of rank, OR
- is annotated `/* @hotpath */`.

**THE CORPUS IS DERIVED FROM A PROFILE, NEVER FROM MEMORY.** No hardware
capture exists for Forsaken yet, so `hotpath_corpus.json` is explicitly
`"provisional": true`: a static seed derived from the frame loop
(`main.c:726` → `RenderScene` `oct2.c:2522` → `MainGame` `oct2.c:5285` →
`MainRoutines` `oct2.c:4551` / `MainGameRender` `oct2.c:5026` →
`RenderCurrentCamera` `oct2.c:5910`) and the ≥256×/frame criterion. The
gate prints that on every run. The first audit deliverable is a real
capture written to `docs/validation/perf-YYYY-MM-DD-*.log`; then set
`provisional: false`, set `captured`, and re-derive the seed from the
profile. A hand-maintained hot list silently stops covering; the corpus is
only as good as its last measurement.

Init, level load, title/menu logic, texture generation, BSP/model parsing,
and teardown are normally COLD (see `cold_always` in the corpus). Do not
"clean up" cold code to satisfy hot-only math rules. If work moves into a
per-frame path, reassess it against the criteria above.

**Adoption baseline.** Forsaken carries years of existing hot-path debt.
`tools/perf/contracts/hot_findings_baseline.txt` and
`r11_candidates_baseline.txt` freeze the adoption-day findings: the gates
PASS today against them, **any new finding fails the build**, and the audit
phase burns the baseline down. Baseline ≠ exemption (§5). A green gate means
"no new debt", never "the tree is clean".

---

## 3. MUST rules (build-blocking)

### R1 — Establish FPSCR FZ + DN on every float thread

Flush-to-Zero (bit 24) and Default-NaN (bit 25) MUST be established on every
thread that executes float math, at thread entry, before any hot path runs.

Forsaken today has **zero FPSCR code**. The only created thread is
`music_thread_fn` (`music_3ds.c:370`, 16 KiB stack), which touches floats in
its channel-restart path (`ndspChnSetRate` `music_3ds.c:323`,
`ndspChnSetMix` `music_3ds.c:327-329`). Upstream libctru's
`initThreadVars()` sets both bits for main and `threadCreate` threads —
verify the linked libctru version; do not assume. The contract still
requires the defensive helpers (T2K precedent):

| Thread | Helper | Call site |
|---|---|---|
| main | `vfp_set_fz_dn()` | start of `main` in `main.c`, before `gfxInitDefault`; failure returns immediately |
| music worker | `vfp_thread_fz_dn()` | first line of `music_thread_fn` in `music_3ds.c` |

Every helper and call site MUST keep a rationale comment: decay can reach
subnormals; FZ/DN are the chosen numerical mode; FPSCR is per-thread;
modern libctru already initializes it on supported entry paths. Unexplained
FPSCR asm is a MUST finding. A new float-touching worker MUST get the same
helper at entry — copy the comment, do not invent a second protocol. FZ/DN
change numerical semantics: never toggle them mid-gameplay, and never claim
the helper is a speedup over initialized libctru state.

### R2 — No float→int conversion in a hot loop body

No float→int casts, `vcvt` sequences, `lroundf`, or array-index-from-float
inside a hot loop without an exemption. Hoist invariant conversions; keep
naturally integer domains integer. An ordinary integer cast is not a
violation; not every `vcvt` is a float→int conversion. Inspect generated
code; never integerize a continuous quantity solely to satisfy a scan.

### R3 — No float comparison driving control flow in a hot loop

`vcmp` + `vmrs` makes ARM control flow depend on VFP flags. New occurrences
in hot loops are restricted pending profiling and a fidelity review.
Forsaken's gameplay hit tests (swept collision windows, lethality checks)
are the protected hit set: pin them with `@vfp-exempt R3`, do not
"fix" them — integerizing a swept window can change the game.

### R4 — No double precision in hot paths

Unexempted `double` arithmetic and promotions are forbidden in hot paths.
Use `float` and `f` literals. Known residue at adoption (burn-down, each
needs a fidelity check before retyping — this is ported original-engine
gameplay math): `spline()` `new3d.c:872-881` (per-entity via `aispline.c`),
`oct2.c:5461` and `models.c:6187` double promotions in effect code,
`EPS`/`M_PI_2` double literals in `new3d.h:17,23`.

### R5 — Loop counters and indices MUST be integer

### R6 — No libm calls in hot paths

Trig goes through `fast_sinf` / `fast_cosf` (`new3d.h:71-84`; 1024-entry
table `new3d.c:22`, built by `trig_table_init` `new3d.c:24-29`, called from
`main.c:490`). One path on every target — do not add `#ifdef __3DS__` trig
forks. `fast_sinf`'s leaf `(int)` truncation and sign fixup are
load-bearing (no float floor on VFPv2; `floorf` is a `bl` with flag
transfers). Do not "clean" it. Domain caveat: `|rad| ≳ 1.3e7` is UB
(`new3d.h:57-61`) — clamp inputs at call sites that can drift.

**Forsaken extension:** `floorf`/`ceilf`/`fmod` count as libm calls here
(§1 item 2). The `FMOD`/`FDIV` sites are the audit's primary burn-down.

`sqrtf` is NOT an R6 violation: with `-fno-math-errno` the ~46 `sqrtf`
them, and do not resurrect the Quake III reciprocal-sqrt bit-hack — it was
tried here and was a pessimisation (VFPv2 has no `VRSQRTE`);
`fast_invsqrtf` is `1.0f/sqrtf(x)` by policy (`new3d.h:86-97`).

### R10 — Build flags follow the devkitPro baseline and are verified

`Makefile.3ds` carries (line 89 today):

```makefile
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
```

plus `-O2` and `-fno-math-errno` (lines 91, 97). These MUST reach the
compile and link commands; the gate checks the Makefile text and the audit
phase verifies effective flags against the installed compiler
(`arm-none-eabi-gcc -Q --target-help`), not from this file.

- Keep `-mtp=soft`: thread-pointer runtime-helper access
  (`__aeabi_read_tp`), unrelated to the float ABI. Never remove it as a
  "float optimization".
- No explicit `-mfpu` is set; devkitARM's armv6k default is VFPv2. Verify,
  don't assume.
- `-fno-math-errno` is what lets `sqrtf` lower to `vsqrt.f32` without errno
  bookkeeping. Code must not depend on math functions setting `errno`.
- **Banned: `-ffast-math`, `-Ofast`, `-fsingle-precision-constant`.** The
  last one would retype every unsuffixed literal in a tree written against
  default double literals — a fidelity change dressed as a cleanup.
- Optimization level changes are a measured decision with a capture.

---

## 3b. TEMPERATURE-INDEPENDENT rules (apply in COLD paths too)

### T1 — Solve, don't search

Do not iterate toward a value that has a closed form. If the condition is
linear (or otherwise directly invertible) in the unknown, invert it. A
search where a solution exists is harder to read, approximate, and hides
its termination condition in a step size.

### T2 — No float comparison in a LOOP CONDITION, at any temperature

`while (x > limit) x -= step;` is a termination hazard: float accumulation
decides the iteration count. Use an integer iteration count, or solve it
(T1). NARROWER than R3 on purpose — R3 covers decisions inside hot loops;
T2 is only about the condition choosing how many times to run.

### The standing exemption for BOUNDED NORMALISATION — pin it, do not fix it

Angle/hue wraps of the shape `while (d > 360.0f) d -= 360.0f;` have a
closed form (`fmod`) and read as T1/T2 violations, but the input is already
near range (one or two iterations), and in ported gameplay math a closed
form is **not bit-identical to repeated subtraction** — a ULP shift moves
gameplay outcomes. Pin them `@vfp-exempt T1`/`@vfp-exempt T2` and move on.
The rules are for code being WRITTEN, not a licence to rewrite ported math
that already works.

---

## 4. SHOULD rules

- **R7** — Division and sqrt: VFPv2 has hardware `vdiv`/`vsqrt`. Prefer
  reciprocal-once outside the loop over per-element divides. Do not table
  them.
- **R8** — Fixed-point domains must not implicitly promote back to float
  mid-loop. No fixed-point domains exist in the tree today; rule is for
  future audio/timer work.
- **R9** — Stay inside the 32-S register file. Doubles and long live ranges
  force spills. Review-only until a capture exists.
- **R11 — INTEGER division or modulo by a RUNTIME value in a hot loop.**
  No integer divide on this core; runtime `%`/`/` are `__aeabi_idivmod`
  calls with a spill barrier. A compile-time-constant divisor is not a
  finding. First known hit: `Random_Range` `new3d.c:593`
  `(Seed1 & 32767) % Max`, called pervasively from per-frame game logic.
  `((x % n) + n) % n` is two helper calls; where the value is provably
  within one period, `if (v < 0) v += n; else if (v >= n) v -= n;` is an
  exact identity — prove the bound per site. Screen:
  `python3 tools/perf/audit_r11_idiv.py` — a SCREEN, NOT A PROOF; a hit is
  a candidate, a clean run means "nothing obvious".

---

## 5. Exemptions

Inline only, this grammar, no paraphrases:

```
/* @vfp-exempt R<id>[,R<id>...] — <reason>. measured <cost-or-n/a>. Verified <YYYY-MM-DD>. */
```

An exemption records a reviewed reason to preserve a site (pixels, hit
tests, rounding, asset output, measured tradeoff). It is not a TODO. Do not
strip pins to "clean up" a file. Do not add hot-only pins to cold code.

Standing exemptions: `tools/perf/contracts/vfp_hotpath.md` §4
(E-LUT-1/2 fast trig range reduction, E-LUT-3 table init, E-LUT-4
fast_invsqrtf, E-TITLE-1 cold title-screen trig).

**Baseline ≠ exemption.** The baseline files are tracked adoption debt with
a ratchet (shrink only); exemptions are reviewed decisions. Do not launder
a baseline entry into an exemption without the review.

---

## 6. What prior perf work established — do not reopen


| Work | Where | Do not undo |
|---|---|---|
| `fast_sinf`/`fast_cosf` LUT, floorf-free range reduction, inlined | `new3d.h:28-103`, `new3d.c:10-29` | no `floorf`, no `__builtin_floorf`, no libm restore |
| Quake III invsqrt removed | `new3d.h:86-97` | VFPv2 has no `VRSQRTE`; the bit-hack was a measured pessimisation |
| ~46 `sqrtf` sites → bare `vsqrt.f32` | tree-wide via `-fno-math-errno` | do not table or hack sqrt |
| hunk allocator (two-tag bump, 8 MB) | `hunk_alloc.c` | bulk-free on level change; keep alignment |
| GPU lighting shader | `Makefile.3ds:107-111`, `shaders/` | replaces CPU `XLight1Group` per-vertex loop |
| Stereo display-list replay disabled; rear camera off | `render_c3d.c:1472-1489`, `oct2.c:5130-5138` | stereo = full CPU traversal twice/frame; that is the measured tradeoff |
| Framebuffers in VRAM; RGBA4 textures | `main_3ds.c:424-450`, `render_c3d.c:42-43` | VRAM budget, not laziness |

**Macro blind spots:** Citro3D macros (e.g. angle converters) can hide
double arithmetic through expansion, and constant folding can remove an
operation entirely. A text scan clears neither — recheck installed headers
and generated code before declaring the binary clear.

---

## 7. Explicit non-rules

Do **not** report:

- An integer loop counter driving float arithmetic in the body.
- Mixed int and float in the same *function* when the hot loop stays in one
  domain.
- Ordinary float arithmetic in COLD PATH as a hot-path finding. R1 and §3b
  still apply there.
- Float comparisons outside a loop as R3/T2 findings.
- Hot-only math findings in code that meets none of §2's three criteria.
- `sqrtf`/`vdiv.f32` in hot paths as R6 (that is R7 review territory).
- libctru's FPSCR init being "redundant" with the R1 helper.

Do **not**:

- Measure a New-3DS-boosted run and call it an OG pass.
- Treat a green `.3dsx` as a perf pass. OG clock, looked-at frame.
- Add `#ifdef` renderer forks; backends are separate files
  (`render_c3d.c` vs `render_gl*.c`), chosen by the build.
- Touch `new3d.h`'s LUT as part of a "quick perf pass".

---

## 8. Scope

Include: root-tree `*.c` / `*.h` compiled by `Makefile.3ds` — the gate
derives scope from `EXCLUDE_SRC` so it follows the build — and
`Makefile.3ds`.

Exclude: `picaGL/**` (vendored), `probe/**`, `tools/**`, `scripts/**`,
`build_3ds/**`, `romfs*/**`, `docs/**`, data/config directories, all
`EXCLUDE_SRC` sources (SDL/DInput/OpenAL/DirectSound/GL backends, enet,
botai, lua_bot), and `breakpad.cpp` / `render_d3d.cpp` (not in the 3DS
build).

---

## 9. C with classes — PHASE 2 (scaffolding live; conversion deferred)

Forsaken is C today (`Makefile.3ds` compiles `.c`; root-level `.cpp` files
are picked up automatically and compile with the language flags below —
`breakpad.cpp`/`render_d3d.cpp` are excluded via `EXCLUDE_SRC` per §8). The
C++ "C with classes" language model — no virtuals, no RTTI, no exceptions,
no project-defined templates, no `std::function`/`std::bind`,
preallocate-in-hot-path, worker stack budgets — is **phase 2**.

**Gates (live now, not on first `.cpp`):**

- `python3 tools/perf/audit_c_with_classes.py` — zero-tolerance language
  screen ported from T2K (`tools/runner/audit_c_with_classes.py`): bans
  `virtual`, `override`, `dynamic_cast`, `typeid`, `throw`/`try`/`catch`,
  `template`, `std::function`/`std::bind`, and the
  `<typeinfo>`/`<exception>`/`<stdexcept>`/`<functional>` includes. Wired
  into `check.sh` as `audit_cxx_class`. Strips comments/strings before
  matching; hits are new code, the fix is to not write the construct —
  there is no baseline file and never should be.
- The numeric gates cover C++ too: `gates_vfp.py` scope is root-level
  `.c/.h/.cpp/.hpp/.cc/.cxx/.hh` minus `EXCLUDE_SRC`. A converted module
  cannot slip past R1–R11 by being a `.cpp`.
- The Makefile compiles `.cpp` with the same numeric flags as `.c` plus
  `-std=gnu++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics`, and
  links with the C++ driver when any C++ object exists. Banned constructs
  do not merely violate the contract — they do not compile.

**Conversion rules (when a module moves from C to C++):**

- A conversion is a translation, not a rewrite. No reordering of float
  comparisons, no literal retyping (never widen
  `-fsingle-precision-constant`), no replacing pinned macros
  (`FMOD`/`FDIV`/`fast_sinf`/`fast_floorf`), no fixed array → container in
  a per-frame path, no allocation or exception in a hot path.
- Every converted module ships with a fidelity A/B: build the module as C
  and as C++, sweep both on hardware with `FIDELITY_DUMP=1`
  (`sdmc:/forsaken_fidelity.txt`, per-tick state checksums;
  `FIDELITY_CHECKPOINTS=1` for tick-level localization), and the checksums
  must be byte-identical. A module that cannot pass its fidelity A/B is
  not converted — that is the merge gate, not a review opinion.
- No user-defined operators on float-bearing types in phase 1: arithmetic
  and comparisons stay explicit and greppable, so the text-based numeric
  gates keep working and the source keeps saying what it does.
- Watch the C++-specific seams the C rules cannot see: implicit narrowing
  in brace-init, overload resolution silently picking a different
  overload, static-init order for globals with constructors.
- `extern "C"` for every header that crosses the C/C++ line.

What the gates still cannot see: per-frame heap growth (violated by
correct shipped design — level/execbuf arrays grow on load) and worker
stack discipline stay review checks. A screen that cries wolf on correct
code is worse than none.

Worker stack budget today: music thread 16 KiB (`music_3ds.c:370`), main
stack 256 KiB (`main_3ds.c:64`). No recursion in the music thread; size
locals against 16 KiB before adding them — overflow there is silent
corruption in the audio path.

---

## 10. 3DS platform requirements

Enforcement: review and hardware validation; the math scripts do not check
P1–P6. Correctness requirements apply in cold paths too.

### P1 — Application lifecycle, sleep, and resume

- The main loop services `aptMainLoop()` every iteration (`main.c:726`) and
  exits when it returns false. Do not replace it with an unconditional loop
  or block it on a worker.
- No custom `aptHook` exists today; suspend/resume is libctru default. If
  gameplay must not advance by wall-clock time spent suspended, rebase the
  frame timer on resume under the existing pause policy before adding
  anything that makes that matter.
- Teardown (`main.c:386-410`) releases acquired resources in dependency
  order: lua → network → music (thread joined, `music_3ds.c:380-396`) →
  sound → platform (`main_3ds.c:589-600`). Initialization failures release
  only what succeeded. Keep that order.

### P2 — Citro3D frame and state ownership

- Draw only after a successful `C3D_FrameBegin`; pair each begin with one
  `C3D_FrameEnd` (`render_c3d.c:426-432`). Preserve both-eye target and
  slider behavior. Gameplay updates once per simulation step, not per eye.
- Rendering state has one owner: the main thread. The music thread never
  touches GPU state. Do not move GPU calls between threads casually.

### P3 — GPU completion, cache visibility, and frame pacing

- Keep submitted buffers/textures valid until the GPU is done; put the
  completion boundary **before** CPU reuse. Current path: Citro3D waits
  for its previous queue in `C3D_FrameBegin`, `C3D_FrameEnd(0)` flushes
  linear memory (`render_c3d.c:430`). Preserve unless an alternative is
  measured.
- The main loop is not vsync-capped (`C3D_FrameEnd(0)`, no VBlank wait);
  `CalculateFramelag` busy-waits only on zero-tick frames
  (`oct2.c:6543-6547`). Do not add redundant VBlank waits or cache flushes
  around the managed path without identifying the need; measure CPU prep,
  GPU work, and waiting separately.

### P4 — Worker synchronization and shutdown

- The one worker (music) is guarded by a `LightLock`
  (`music_3ds.c:305,337,368`), joined before teardown, 25 ms poll
  (`music_3ds.c:338`). `volatile` alone is not synchronization. New workers
  need: R1 helper, documented ownership, joinable lifetime, stack sized
  against its role. Do not add threads as an unmeasured performance fix.

### P5 — RomFS, SD access, and streaming

- RomFS mount precedence embedded → SD (`main_3ds.c:354-396`), then
  `chdir("romfs:/")`. Writable saves stay on the writable path. Check
  open/read results; distinguish a complete asset from a short read.
- **No blocking SD I/O in the frame loop.** Each trace write is 5–10 ms;
  assets; stream large data with a bounded budget.

### P6 — Evidence for changes to platform paths

| Area changed | Required evidence |
|---|---|
| Build or math | Clean build commands, tool/library versions, relevant disassembly, fidelity checks, comparable hardware capture |
| Rendering or sync | Busy-scene pacing, both-eye/slider behavior, buffer reuse under load, clean teardown |
| Lifecycle or workers | HOME return, lid sleep/wake, repeated pause/resume, clean exit, no stuck waits or duplicate workers |
| Filesystem or streaming | Packaged assets in each shipped format, missing/read-error handling, streaming under load |

Record hardware, scene/workload, clock/cache settings, build revision, and
stereoscopic mode. If original hardware is unavailable, state the gap; do
not claim it passed. A documentation-only revision is not a performance
pass.

---

## 10b. Autonomous hardware loop — netload_stub (no FTP, no hands)

Full runbook: port-docs vault `20-runbooks/3ds-autonomous-hardware-profiling-loop.md`.
This is THE way to collect hardware data; FTP/ftpd-pro is not part of the loop.

Mechanism (all on the 3DS, one human launch of `sdmc:/3ds/netload_stub.3dsx`):

1. **Push**: `3dslink -a <3DS-IP> forsaken3ds.3dsx` — the stub (TCP :17491) saves to
   `sdmc:/3ds/` and chain-loads it via Luma `hb:ldr`.
2. **Run**: the autotest build (`EDITION=remaster AUTOTEST_REMASTER=1`) sweeps levels,
   then chain-loads back to the stub itself (`autotest_chainload_to_stub`, driven by
   `QuitRequested` so normal teardown — and the PERF dump — still runs).
3. **Completion signal**: **:8080 down then up**. The stub serves `GET http://<IP>:8080/<path>`
   from `sdmc:/` (404 = absent — the clean absent-vs-empty discriminator). Poll the port;
   never sleep a fixed duration.
4. **Pull results over HTTP**: `curl http://<IP>:8080/forsaken_perf.txt` (also
   `forsaken_trace.txt`, `forsaken_boot.log`). Verify freshness by md5 — an unchanged
   hash means the run wrote nothing; analyze stale data at your peril.
5. Re-push for the next cycle. `tools/perf/ralph_loop.sh <N>` automates N cycles.

Rules:
- **Never wait on a human to report loop state.** When a loop or device wait is in
  flight, schedule a wakeup or a watcher process that acts when the condition fires.
- Capture builds must not enable `VERBOSE_TRACE` (per-line fsync poisons timings);
  `PERF_CAPTURE=1` writes `sdmc:/forsaken_perf.txt` at teardown via its own fd.
- The Makefile does not track flag changes — `make clean` (or touch every affected
  TU) on any `-D` flip, or a stale object silently relinks.
- The harness never ships: autotest/PERF builds are branch-local and must not be
  merged to a release branch.

---

## 11. References and revision record

1. ARM11 MPCore TRM (DDI 0360F), §19.11 / Table 19-16.
2. VFP11 TRM (DDI 0274H), register organization, numerical modes.
3. devkitPro application Makefile template (ARCH baseline).
4. GCC ARM options (`-mtp`, `-mfpu`); GCC optimization options
   (`-fno-math-errno`, `-ffast-math`).
5. libctru `syscalls.c` `initThreadVars()` (FZ/DN init — verify linked
   version), `apt.c` (`aptMainLoop`).
6. Citro3D `renderqueue.c` (frame sync, queue completion, cache flush).
7. T2K `AGENTS.md` rev 2026-10-14/amended 2026-10-19 — contract shape and
   rule lineage; T2K's project-frozen waiver does **not** apply here.
   allocator), `3DS_PORT_NOTES.md` (memory history), `new3d.h` header
   comments (LUT rationale).

### Changes

- 2026-10-26: §9 C-with-classes moved from "commitment" to "scaffolding
  live": T2K's `audit_c_with_classes.py` ported to `tools/perf/` and wired
  into `check.sh` (`audit_cxx_class`); `gates_vfp.py` scope widened to the
  C++ extensions so R1–R11 cover converted modules; `Makefile.3ds` compiles
  root-level `.cpp` with `-std=gnu++17 -fno-exceptions -fno-rtti` and links
  with the C++ driver when C++ objects exist; conversion rules added
  (translation-not-rewrite, per-module fidelity A/B merge gate). Build-flag
  stamp added to `Makefile.3ds` so `-D` changes force a full rebuild
  (three stale-object incidents this session).
- 2026-10-25: contract adopted from T2K's shape and re-derived for Forsaken.
  Hot corpus is a provisional static seed (no capture yet). R6 extended to
  `floorf`/`ceilf`/`fmod` (this tree's libm debt). R1 recorded as an open
  gap (no FPSCR code). Adoption-debt baseline ratchet introduced. §9 C-with-
  classes deferred to phase 2. Tooling ported to `tools/perf/`.
