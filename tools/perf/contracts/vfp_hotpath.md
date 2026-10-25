# VFP Hot-Path Contract — Forsaken 3DS

**Target:** Nintendo 3DS (ARM1176JZF-S, VFP11 / VFPv2)
**Scope:** every `.c`/`.h` compiled into the 3DS build by `Makefile.3ds`
(root tree minus `EXCLUDE_SRC`). See §7.
**Status:** Enforceable — agents MUST report violations by rule ID
**Shape:** adopted from T2K's `AGENTS.md` contract (2026-10-25). Forsaken is
pure C; the C++ "C with classes" half is deferred to phase 2 (AGENTS.md §10).

---

## 0. Rationale

The ARM11 has a real hardware FPU (VFP11, VFPv2, IEEE 754). Raw
floating-point arithmetic is not the problem. Four things are:

1. **The coprocessor boundary.** ARM↔VFP transfers, `vcvt` conversions, and
   `vcmp`+`vmrs` flag reads are each their own cost (ARM11 MPCore TRM
   §19.11 Table 19-16: 1-cycle throughput, 2–8 cycle latency), and a
   dependency chain across the boundary serializes a loop. libm calls that
   cross it per iteration — `floorf` is the live example in this tree —
   dominate the loop.
2. **Trap-to-support-code on exceptional values.** Denormals stall unless
   flush-to-zero is enabled. Forsaken currently sets no FPSCR anywhere; it
   relies on libctru's `initThreadVars()`. See R1.
3. **Register file size.** 32 single-precision registers; doubles consume
   two. Spills are memory traffic on a core with 16 KB L1D and no L2 (OG).
4. **No integer divide.** ARMv6 has no `sdiv`/`udiv`. Runtime integer `/` or
   `%` is a `bl __aeabi_idiv`/`__aeabi_idivmod` — tens of cycles plus a call
   barrier that spills live registers. VFPv2 *does* have hardware `vdiv`/
   `vsqrt`, which is why R11 is about integer division specifically.

This contract does **not** prohibit floating-point math, and it does **not**
prohibit mixing integer and float types in the same function.

---

## 1. Definitions

**HOT PATH** — a function or loop body that satisfies at least one of:
- Executes per-frame at a rate ≥ 1× frame count, AND appears in the top 15
  entries of a captured profile, OR
- Executes ≥ 256 times per frame regardless of profile rank, OR
- Is explicitly annotated `/* @hotpath */`

While `hotpath_corpus.json` carries `"provisional": true`, the corpus is a
static call-graph seed (frame loop + ≥256×/frame), not a profile, and the
first criterion is dormant. The gate says so on every run. Nothing in this
contract applies to code that is not HOT PATH. See §5.

**BOUNDARY CROSSING** — any operation that moves a value between the ARM
integer register file and the VFP register file, or that requires a VFP
status flag to be read by ARM-side control flow. Includes:
- `vmov` / `vmrs` between register files
- float→int and int→float conversion (`vcvt`)
- float comparison whose result drives a branch
- float value used as an array index

**COLD PATH** — everything else. Init, level load, menu/title logic, texture
generation, BSP/model parsing, teardown.

---

## 2. Rules

Severity: **MUST** = build-blocking violation. **SHOULD** = review finding:
reported separately from blockers, but still fails the ratchet until fixed
or explicitly blessed (§8).

### R1 — FPSCR configuration (MUST)

Flush-to-Zero (bit 24) and Default-NaN (bit 25) MUST be established on every
thread that executes float math, at thread entry, before any hot path runs —
either by verified libctru initialization or by an explicit helper with a
read-back.

Forsaken today has **no FPSCR code at all** (audit 2026-10-25). The only
created thread is `music_thread_fn` (`music_3ds.c:370`, 16 KiB stack), which
touches floats in its channel-restart path (`ndspChnSetRate`,
`ndspChnSetMix`). Upstream libctru's `initThreadVars()` sets FZ+DN for main
and `threadCreate` threads — verify the linked libctru version rather than
assuming. The contract requires the defensive helper anyway, per T2K
precedent: `vfp_set_fz_dn()` at the start of `main` (failure returns before
`gfxInitDefault`), `vfp_thread_fz_dn()` as the first line of
`music_thread_fn`, each with a rationale comment. Uncommented FPSCR asm is a
MUST finding; a new float-touching worker without the helper is a MUST
finding.

FZ/DN change numerical semantics: do not toggle them mid-gameplay and do not
claim the helper is a speedup over already-initialized libctru state — it is
a correctness pin, not an optimization.

### R2 — No float→int conversion in a hot loop body (MUST)

### R3 — No float comparison driving control flow in a hot loop (MUST)

### R4 — No double precision in hot paths (MUST)

Use `float` and `f`-suffixed literals. Known residue at adoption:
`new3d.c:872-881` `spline()` computes Catmull-Rom coefficients in `double`
and runs per-entity via `aispline.c`; `oct2.c:5461` and `models.c:6187`
promote to double inside throttled effects. `new3d.h:17,23` `EPS`/`M_PI_2`
are double literals. Each is a burn-down item, not a licence to add more.
Forsaken's gameplay math is ported original-engine code: retyping a
gameplay predicate is a fidelity change and needs a fidelity review, same
bar as T2K's mechanics-gated rule.

### R5 — Loop counters and indices MUST be integer

### R6 — No libm calls in hot paths (MUST)

Presentation / camera / mesh / AI trig goes through `fast_sinf` / `fast_cosf`
(`new3d.h:71-84`, table `new3d.c:22`, init `trig_table_init` called from
`main.c:490`). The table is one path on every target. `fast_sinf`'s leaf
`(int)` truncation and sign fixup are load-bearing — VFPv2 has no
float-result floor, and `floorf` is a real `bl` into newlib with VFP↔ARM
flag transfers (`new3d.h:39-61`). Do not "clean it up".

This tree's extension of T2K's rule: **`floorf`/`ceilf`/`fmod` count as libm
calls here**, because they are this tree's actual hot-path libm debt — the
`FMOD`/`FDIV` macros (`new3d.h:105-106`) expand to `floorf` inside per-entity
loops (`models.c`, `secondary.c`, `2dpolys.c`, `bgobjects.c`, `spotfx.c`,
`rtlight.c`, `pickups.c`) and `mxaload.c:1195-1199` runs four per vertex in
`InterpFrames`. Burn-down list, not a licence.

`sqrtf` is NOT R6: with `-fno-math-errno` the ~46 `sqrtf` sites lower to bare
square root — the Quake III bit-hack was tried here and was a pessimisation
(VFPv2 has no `VRSQRTE`).

`fast_invsqrtf` is `1.0f/sqrtf(x)` by policy (`new3d.h:86-97`).

### R7 — Division and sqrt SHOULD be avoided in hot paths

VFPv2 has hardware `vdiv`/`vsqrt`. Prefer reciprocal-once outside the loop.
Do not table them.

### R8 — Fixed-point domains MUST NOT implicitly promote

Forsaken has no fixed-point domains today; rule retained for future audio/
timer work.

### R9 — VFP register pressure SHOULD stay under budget

32 S registers; count generated loads/stores in a disassembly, not source
variables. Review-only until a capture exists.

### R11 — Integer division/modulo by a runtime value (SHOULD)

No integer divide on this core. `new3d.c:593` `Random_Range` does
`(Seed1 & 32767) % Max` and is called pervasively from per-frame game logic —
the first burn-down item. A compile-time-constant divisor is NOT a finding.
`((x % n) + n) % n` is two helper calls; where the value is provably within
one period, a conditional add/subtract is an exact identity — prove the bound
per site. Screen: `python3 tools/perf/audit_r11_idiv.py`.

### R10 — Build flags MUST be verified, not assumed

`Makefile.3ds` must carry
`ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft`
(line 89 today) and `-fno-math-errno` (line 97 today), and MUST NOT carry
`-ffast-math`, `-Ofast`, or `-fsingle-precision-constant`.

- `-mtp=soft` is thread-pointer runtime-helper selection, not a float flag.
  Do not remove it as a "float optimization".
- No explicit `-mfpu` is set; devkitARM's armv6k default is VFPv2. Verify
  effective settings with the installed compiler (`arm-none-eabi-gcc -Q
  --target-help`), not from this file.
- `-fno-math-errno` is what lets `sqrtf` lower to `vsqrt.f32` without errno
  bookkeeping. Code must not depend on math functions setting `errno`.
- `-fsingle-precision-constant` is banned here because this tree was written
  against default double literals; adding it would retype thousands of
  unsuffixed literals at once — a fidelity change, not a cleanup.
- Optimization is `-O2`; changing it is a measured decision with a capture,
  not a preference.

---

## 3. Explicit non-rules

The following are **NOT** violations and MUST NOT be reported:

- An integer loop counter driving float arithmetic in the body.
- Mixed int and float types in the same *function*, where the hot loop
  itself stays in one domain.
- Float math in COLD PATH code of any shape.
- Float comparisons outside a loop.
- Any pattern in code the corpus/profile does not show as hot.
- `sqrtf`/`vdiv.f32` hardware ops in hot paths (that is R7 review territory,
  not R6).
- libctru's own FPSCR initialization being "redundant" with the R1 helper —
  the helper is a correctness pin against toolchain drift.

---

## 4. Exemptions

An exemption is granted inline, this grammar only, no paraphrases:

```c
/* @vfp-exempt R3 — predicate depends on per-vertex data that cannot be
   hoisted; measured cost 0.4ms, accepted. Verified 2026-01-01. */
```

```
/* @vfp-exempt R<id>[,R<id>...] — <reason>. measured <cost-or-n/a>. Verified <YYYY-MM-DD>. */
```

An exemption records a reviewed reason to preserve a site (pixels, hit tests,
rounding, asset output, measured tradeoff). It is not a TODO. Do not strip
pins to "clean up" a file. Do not add hot-only R-rule pins to cold code.

Standing exemptions (corpus, not source comments):

| ID | File | Rule | Standing | Why |
|---|---|---|---|---|
| E-LUT-1 | new3d.h:fast_sinf | R2,R3,R5 | yes | Range reduction; leaf vcvt+flag; documented cheaper than floorf |
| E-LUT-2 | new3d.h:fast_cosf | R2,R3 | yes | Wraps fast_sinf; same range reduction |
| E-LUT-3 | new3d.c:trig_table_init | R4,R6 | yes | COLD table build; libm sinf is the reference |
| E-LUT-4 | new3d.h:fast_invsqrtf | R7 | yes | 1/sqrtf is the shipped policy (VFPv2 vsqrt) |
| E-TITLE-1 | title.c | R6 | yes | Title/menu is COLD; ATAND/atan2f at title.c:4697 is one cold site |

**Baseline ≠ exemption.** `contracts/hot_findings_baseline.txt` and
`contracts/r11_candidates_baseline.txt` are the adoption-day debt list:
existing findings that pass today but may not grow. Baseline entries are a
burn-down list — shrinking them is the audit phase's work; growing them is a
build failure. Exemptions are reviewed decisions; baselines are tracked debt.
Do not move a finding from baseline to an exemption without the review an
exemption requires.

---

## 5. Applicability

If a function is not HOT PATH, emit nothing (hot-only rules R2–R9, R11).
R1 and R10 have their own wider scope.

---

## 6. Profile ingest

HOT PATH = union of `/* @hotpath */`, `hotpath_corpus.json` seed, parsed
`docs/validation/perf-*.log` top-15 stages, and static ≥256×/frame loops.
No capture exists yet; the seed is provisional and the gate prints that
every run until a capture replaces it.

---

## 7. Scope

Include: root-tree `*.c` / `*.h` compiled by `Makefile.3ds` (scope is
derived from `EXCLUDE_SRC` in the Makefile, so it follows the build),
`Makefile.3ds`.

Exclude: `picaGL/**` (vendored), `probe/**`, `tools/**`, `scripts/**`,
`build_3ds/**`, `romfs*/**`, `docs/**`, data/config directories, and every
source in `EXCLUDE_SRC` (SDL/DInput/OpenAL/DirectSound/GL1-3 backends,
enet/net tracker, botai, lua_bot). `breakpad.cpp` and `render_d3d.cpp` are
not compiled into the 3DS build and are out of scope.

---

## 8. Report schema

Every finding has `rule`, `severity`, `file`, `line`, `symbol`, `hotpath`,
`hotpath_reason`, `exemption`, `detail`.
FAIL iff any unexempted finding is not in the adoption baseline —
MUST findings are reported as blockers, SHOULD findings as reviews, but
both fail the ratchet: a new SHOULD finding is fixed or explicitly blessed
by a reviewer, never waved through. A green gate means "no new debt",
never "the tree is clean".
