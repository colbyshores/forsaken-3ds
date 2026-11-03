    # TASK: On-device profiling and optimization loop on a physical Old 3DS

    ## HARNESS CONTRACT — read this first

    You are ONE iteration of an autonomous loop (ralph.sh re-launches
    a fresh agent with this same prompt repeatedly). You have no
    memory of previous iterations except what is on disk. Rules:
    - On every invocation: read ./perf/OPTIMIZATION-LOG.md (if it
      exists), `git status`, `atm status`, and `atm journal` BEFORE
      doing anything. Disk state decides which phase you are in —
      never assume, never restart completed phases.
    - Do exactly ONE phase or ONE loop iteration (as defined below),
      write your results to the log/state files, then END YOUR TURN.
      The harness will relaunch you. Never run the whole campaign in
      one turn.
    - If ./perf/LOOP-DONE.md exists: do nothing except confirm and end.
    - If the console or log state contradicts what you expected
      (unresponsive console, leftover crash artifacts, a change
      present in the tree with no log entry), STOP: write what you
      found into ./perf/BLOCKED.md with exact commands/output, and end.
      Do not improvise recovery and do not push on blind.

    ## Mission

    You have access to a physical Original Nintendo 3DS (Old 3DS ONLY —
    New 3DS numbers are never acceptable evidence) running a custom
    Luma3DS+ATM firmware that makes it an autonomous hardware-testing
    target. We profile on the OG 3DS because it is the lowest common
    denominator: if it runs well here, it runs well everywhere this
    project targets — and performance claims from any faster platform
    prove nothing about this one. Your job across iterations: profile
    THIS project running on that real hardware, find every
    optimization you can justify with on-device measurements, apply
    them one at a time, and prove each one helped.

    ## Step 0 — Learn the hardware interface before touching anything

    The complete operational manual is in the Obsidian vault (use the
    Obsidian MCP tools): start with "ATM Autonomous Agent Runbook —
    Original Old 3DS" and follow its links to `network-api-reference`,
    `deployment-and-launch`, `debugger-guide`,
    `profiling-and-telemetry`, `developer-mode`, `artifact-management`,
    and `troubleshooting`. Do not invent endpoints or commands — if the
    runbook doesn't document it, it doesn't exist. The firmware/tooling
    source of truth is the Luma3DS repo at /mnt/nas/Development/Luma3DS
    (controller: `tools/atmctl.py`, instrumentation core:
    `sysmodules/rosalina/source/atm/atm_core.c`, example workload:
    `tests/workloads/atm_workload_ok.c`).

    Console facts (verify, never assume): 3DS at 192.168.4.167,
    workstation 192.168.4.220, enrolled device id `old3ds-lan167`,
    firmware build `da4b1b95a66c`. Ports: HTTP 8080, deploy TCP 17491,
    telemetry UDP 8081, commands UDP 8082, GDB 4000–4003.

    Define once per shell:
      alias atm="python3 /mnt/nas/Development/Luma3DS/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167"

    ## Phase machine (do the FIRST phase whose output is missing)

    Phase A — VALIDATE the hardware loop.
    Output: ./perf/OPTIMIZATION-LOG.md created with console identity
    section + smoke-test result.
    This ATM build is source/host-verified but has little physical
    runtime so far. Before trusting any number:
      atm status        # device must match, operational=yes
      atm devmode       # devmode_armed=yes; ideally devmode_active=yes
                        # svc_gdb=active:* required before any debugging
      atm artifacts     # what's pending from previous work
      atm journal       # previous run state
    Then smoke-test with a trivial existing workload if available
    (atm-deploy/workloads/atm_workload_ok.3dsx in the Luma3DS repo):
      atm run <workload.3dsx> ./perf/smoke --build-id smoke
    Only mark Phase A complete when deploy→launch→run→retrieve
    completes with outcome COMPLETED and artifacts verified. If any
    stage fails, diagnose via the runbook troubleshooting page, write
    ./perf/BLOCKED.md with the exact failure, and end — do not build
    profiling conclusions on a partially working loop. Hard
    hangs/lockups are OUTSIDE the recovery contract: if the console
    stops answering entirely, record manual-intervention-required in
    BLOCKED.md; never claim recovery.

    Phase B — INSTRUMENT this project.
    Output: profiling build exists; log records how to build it and
    its build-id.
    ATM measures nothing generically — no CPU%, no GPU, no memory
    headroom. Frame timing comes from the app itself:
    - Link `atm_core.c` (+ its header) from the Luma3DS repo into a
      dedicated profiling build of this project.
    - Call `atm_frame_add()` once per frame in the main loop (O(1),
      no allocation, no I/O in the hot path). Add any app-specific
      counters you need the same way (bounded, aggregated).
    - After a FIXED, deterministic workload (same scene/input/frame
      count every run — if the project has no benchmark mode, add one
      behind a build flag; never profile interactive play), write the
      `atm_frame_format_summary()` line plus your counters to
      `/3ds/atm_result.txt`, then exit cleanly.
    - Keep the `.elf` next to every `.3dsx` build. Wrong-build symbols
      are the #1 debugging hazard; the GDB wrapper verifies sections,
      and you must treat a mismatch as fatal to that session.
    Measurement honesty: `osGetTime` granularity is 1 ms and
    percentiles are bucket estimates. Say so in every report. Never
    extrapolate to CPU/GPU utilization.

    Phase C — BASELINE before optimizing.
    Output: log contains a baseline table of 3 runs.
      atm run build/<app>.3dsx ./perf/baseline-<gitrev> \
          --build-id <gitrev-or-content-id> --scenario <workload-name>
    Run the baseline 3 times. Record per run: build_id, firmware_id
    (from `atm status`), scenario, frame summary (count, avg, p50,
    p95, p99, max, over-budget count), and outcome. The spread across
    the 3 baselines is your noise floor: no "improvement" smaller than
    run-to-run noise counts as an improvement. Commit nothing yet.

    Phase D — ONE OPTIMIZATION ITERATION (repeatable, see protocol).

    ## First profiling target — stereo traversal cost (start Phase D here)

    Prior finding to VERIFY in this project's source, not assume:
    "Single-pass stereo replay is DISABLED. The replay mechanism
    recorded left-eye draws into s_dl[] and re-submitted them with
    the right eye's matrix/state, but the engine's gameplay/
    visibility traversal in oct2's eye callbacks run fully (CPU cost
    ~2x 3D traversal in stereo mode), so the CPU savings were not
    realized and the replay path was disabled. The eye callbacks
    still run fully in stereo."

    Investigate in this order (each numbered item may take its own
    Phase D iteration — log partial findings before ending your turn):
    1. Confirm or refute the claim in code: locate s_dl[], the
       replay submit path, the eye callbacks, and what work
       (gameplay? visibility/PVS? draw collection?) actually runs
       per eye vs per frame. Report where the quote is stale.
    2. Baseline BOTH configurations as separate scenarios with
       identical scene/inputs: mono (3D off) and stereo (3D on).
       Add bounded app-side counters beyond frame time: per-eye
       traversal invocation count and accumulated ms, draw-call
       count per eye, s_dl recorded size. Frame time alone cannot
       attribute this cost — do not guess the split.
    3. Candidate fixes, as separate loop iterations, smallest first:
       (a) hoist eye-invariant work (gameplay, visibility) out of
           the per-eye callbacks so it runs once per frame and only
           draw submission is per-eye;
       (b) only if (a) is insufficient or inapplicable, evaluate
           re-enabling replay — note the original failure mode:
           replay saves draw-submission CPU but is worthless if
           traversal still runs twice, so quantify replay overhead
           vs second-traversal cost before reviving it.
    4. Correctness for stereo changes MUST use a 3D screenshot set
       (manifest is3d=yes, includes the right-eye image): both eyes
       correct, correct disparity, no left-eye state leaking into
       the right eye. A mono-only screenshot does not verify this.
    Success criterion: stereo frame cost approaching mono + draw
    submission only, proven by the per-eye counters, not just FPS.

    ## Phase D iteration protocol (self-directed loop)

    State lives on disk, not in your context:
    - At the START of every iteration, re-read
      ./perf/OPTIMIZATION-LOG.md and run `atm status` + `atm journal`.
      Never re-attempt a hypothesis already marked reverted/dead.
      If the console state doesn't match what the log expects (wrong
      run, pending crash artifacts, not operational), reconcile or
      STOP (write ./perf/BLOCKED.md) — never push on blind.
    - Each iteration completes fully before the next begins:
      hypothesis → one change → 3 measured runs → correctness →
      keep/revert → log entry written BEFORE you end your turn.
    - Stop conditions (any one ends the campaign: write the final
      summary into the log, create ./perf/LOOP-DONE.md, and end):
      (a) 3 consecutive iterations with no gain beyond baseline noise;
      (b) 15 total Phase D iterations (count them from the log);
      (c) console unresponsive or any INCOMPLETE_RECOVERY (also write
          BLOCKED.md instead of LOOP-DONE.md in this case);
      (d) the only remaining ideas require a visual/gameplay trade-off
          or a mass rewrite — write those as proposals in the log for
          my approval instead of implementing them.
    - You own task selection between iterations: always attack the
      largest measured cost next (stereo traversal first, per the
      target section). If your context is compacted or restarted,
      the log + `atm journal` are how you recover — trust disk over
      memory.

    One Phase D iteration is:
    1. HYPOTHESIZE from evidence, not vibes. Use the frame stats and
       counters to name the suspected bottleneck. To localize it, use
       the debugger (diagnosis ONLY — see rules below):
         atm deploy build/<app>.3dsx --build-id <id> --no-launch
         atm launch <name>   # then confirm `running` via atm journal
         python3 /mnt/nas/Development/Luma3DS/netload_stub/host/n3dlgdb.py \
           192.168.4.167 --elf build/<app>.elf --port 4000 --cmds cmds.txt
       or raw arm-none-eabi-gdb extended-remote :4000 (break-on-start
       via :4003 before launch). Breakpoints, watchpoints, register/
       memory inspection, and backtraces are for understanding WHERE
       time goes. Detach cleanly when done.
    2. CHANGE exactly one thing. Smallest diff that tests the
       hypothesis. Respect Old 3DS limits: no unbounded allocation,
       no new threads casually, no SD writes or network in hot paths.
    3. MEASURE: same scenario, same frame count, 3 runs via `atm run`
       into a fresh ./perf/<change-id>/ dir. Compare median-of-3 and
       p95/p99 against baseline median — averages alone hide hitches.
    4. VERIFY CORRECTNESS, not just speed:
         atm screenshot --label <change-id> --retrieve-to ./perf/<change-id>/shots
       Compare screenshots against baseline visually (rendering must
       not change unless the change intends it). Screenshots stall the
       workload (whole-cache flush) — capture in a SEPARATE run from
       timed runs, never during measurement.
    5. KEEP or REVERT based on the numbers. A change with no
       measurable gain beyond noise, or any correctness regression,
       gets reverted. Log it either way (hypothesis, diff summary,
       before/after table, keep/revert) in ./perf/OPTIMIZATION-LOG.md.
    6. Use your resources. You have Internet access plus Obsidian MCP
       and Cognee MCP. Use them aggressively to GENERATE hypotheses —
       known Old 3DS/ARM11 bottlenecks, PICA200 costs, libctru/citro3d
       pitfalls, prior findings from memory. But external claims are
       never evidence: every researched idea still enters at step 1
       of this iteration, one variable at a time, and lives or dies
       by this console's measurements. Record the source of each
       hypothesis in the log. Work autonomously and make good
       engineering decisions within these rules; never trade visual
       quality or gameplay behavior for frame time without flagging
       it as a proposal for my approval instead of implementing it.
       If you write findings to Cognee/memory, write only measured
       results, labeled with build_id + firmware_id — never store
       hypotheses as facts.

    ## Crashes during the loop

    They will happen; use them, don't fear them. `atm run` already
    handles supported crashes: unattended capture (fatal ARM11 and
    ErrDisp paths), auto-reboot, reconnect, artifact retrieval, and
    correlation to the run/build. Outcomes CRASH_FATAL/CRASH_ERRDISP
    mean recovery worked; INCOMPLETE_RECOVERY/RECOVERY_FAILURE mean
    stop and report (BLOCKED.md). Resolve crash PCs against THAT run's
    exact ELF:
      arm-none-eabi-addr2line -e build/<app>.elf <pc>
    A missing dump is never proof no crash occurred. Never ack/delete
    artifacts except through atmctl's verify-then-ack flow.

    ## Hard rules

    - NEVER report debugger-attached or screenshot-interrupted runs
      as performance evidence.
    - NEVER substitute emulator, New 3DS, or host timings for Old 3DS
      measurements. Label every number with build_id + firmware_id +
      scenario, or don't report it.
    - One variable at a time. No speculative mass rewrites.
    - Preserve behavior outside the profiling build flag; the release
      build must keep working exactly as before, minus the measured
      optimizations.
    - Do not modify the Luma3DS firmware, atmctl, or the console's
      SD gate files. If the hardware interface itself seems broken,
      stop and report it (BLOCKED.md) — that's a different project's bug.
    - Do not commit or push anything without my explicit approval.
      (The loop harness does NOT change this: no per-iteration
      commits. Your checkpoint is the log + measured artifacts.)
    - If the console becomes unresponsive (no HTTP, no telemetry,
      past `atm wait-ready` budget), stop and report exactly what
      was running (BLOCKED.md). Do not loop retries forever.

    ## Deliverable (assembled incrementally; finalized in LOOP-DONE.md)

    1. ./perf/OPTIMIZATION-LOG.md: baseline table (3 runs), then one
       entry per attempted change — hypothesis, evidence that
       motivated it, diff summary, before/after medians (avg, p50,
       p95, p99, max), correctness check, keep/revert decision.
    2. A final summary (in LOOP-DONE.md): total measured improvement
       vs baseline (per metric), which changes survived, what you
       tried that didn't help, what remains unmeasured or blocked,
       and anything about the hardware loop itself that surprised you.
    3. All kept optimizations as an uncommitted working-tree diff for
       my review, listed file by file in the final summary.

    Begin: determine your phase from disk, do that ONE phase or ONE
    iteration, log it, and end your turn. Find every optimization the
    hardware can prove exists — and claim none it can't.

