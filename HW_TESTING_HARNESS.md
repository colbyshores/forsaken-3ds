# Forsaken 3DS — Hardware Testing Harness Runbook

How to get a Forsaken build onto real 3DS hardware and what each path can and
cannot prove. Written from a working session (2026-11-21) so the next agent
does not re-derive the gotchas.

There are **two distinct delivery paths** for **two different jobs**. Pick by
what you need to verify.

| Path | Artifact | Console state | Proves | Does NOT prove |
|---|---|---|---|---|
| **A. netload_stub** (3dslink) | thin `.3dsx` | boots into a netboot **stub**, headless | boots to main loop, init + first frame execute, no crash, frame rate | interactive input (no display foreground, parked at frame 1) |
| **B. ftpd-pro** (FTP + FBI) | `.cia` | normal CFW, launched from HOME | full interactive gameplay: keyboard, sticks, overlay, persistence | — (this is the functional path) |

**Read the source-of-truth docs first** (do not improvise against the console):
- `/mnt/nas/Development/netload_stub/AGENTS.md` — standing hardware contract.
- `/mnt/nas/Development/netload_stub/forsaken-AGENTS-10b-proposed.md` — Forsaken harness workflow.
- `/mnt/nas/Development/forsaken/banner/README.md` — ftpd-pro CIA push example.

---

## Console inventory

- **netload_stub appliance:** `old3ds-lan167` @ `192.168.4.167` (Original Old 3DS).
  Runs the netboot stub (TCP `17491`, HTTP `8080`). Cannot exercise New-3DS
  C-stick (no nub, no Circle Pad Pro attached).
- **ftpd-pro console:** a *different* console running the `ftpd-pro` homebrew
  FTP server. Its IP changes per session — **ask the operator for it** (this
  session used `192.168.4.90`). ftpd-pro default: **port 5000, anonymous login**.

`atmctl.py` is the only sanctioned controller for the stub appliance. Always
pin identity with `--device-id` (it **fails closed** on mismatch — never drop it):

```
ATM="python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167"
```

Subcommands: `discover status journal artifacts wait-ready deploy launch del cmd reboot fwupdate retrieve run screenshot devmode gdb-info`.

---

## Preflight (always, before touching the stub)

```
$ATM status    # expect operational=yes, phase=listening, busy=no, recovery_ready=yes
$ATM journal   # note boot_count; a reboot must increment it
```

---

## Workflow A — netload_stub thin-build boot smoke test (headless)

Use to prove a build **boots and runs without crashing** on real hardware
(e.g. that new init code like `irrstInit` is safe on OG). It is **not** a
functional input test.

### 1. Build the ELF, then a THIN .3dsx (the key trick)

Build normally, then convert the ELF to a `.3dsx` **without romfs** (~3.5 MB):

```
export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM
export PATH="$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH"
make -f Makefile.3ds                      # -> build/3ds/forsaken3ds.elf (+ full .3dsx)
3dsxtool build/3ds/forsaken3ds.elf /tmp/kilo/fs-thin.3dsx   # NO --romfs => ~3.5 MB
```

> **GOTCHA — do NOT push the full-romfs `.3dsx` (~284 MB) over 3dslink.** It
> stalls the stub receiver with `net_eof_or_stall (no transfer stats)` every
> time (confirmed 3x). The thin build pushes in ~3 s and boots through
> `AppInit: SUCCESS → first RenderScene`. The game reaches the render loop
> even without romfs; it just parks at frame 1 (the §7 GSP foreground wall).

### 2. Deliver (3dslink is the only sanctioned delivery to a live stub)

```
/opt/devkitpro/tools/bin/3dslink -a 192.168.4.167 /tmp/kilo/fs-thin.3dsx
```

Verify the device actually took it (the client's progress % lies — trust the log):

```
curl -s http://192.168.4.167:8080/3ds/netload_stub/transfers.log | grep fs-thin
# want: status=ok bytes_out=<exact size>
```

### 3. Read the evidence

```
curl -s http://192.168.4.167:8080/forsaken_boot.log | grep -iE "appinit|main loop|first render|first fsbegin|error|fail|assert|crash"
curl -s http://192.168.4.167:8080/forsaken_boot.log | tail    # live "FPS: 59" lines = running
$ATM journal   # crash_class empty, exception_path=none
```

Pass = `AppInit: SUCCESS — entering main loop` → `first RenderScene` →
`first FSBeginScene`, steady FPS, no crash lines.

### 4. Restore the stub (chainload kills the stub; terminate is ineffective)

```
$ATM reboot --wait 180     # boot_count must increment; then re-check status
```

---

## Workflow B — ftpd-pro CIA install (interactive functional test)

Use to actually exercise input features: system keyboard, analog sticks,
controls overlay, config persistence. A normally-launched CIA has full display
and interactive input, unlike the headless stub.

### 1. Build the CIA (LOMEM = runs on OG and New 3DS)

```
export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM
export PATH="$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH"
make -f Makefile.3ds cia      # -> build/3ds/forsaken3ds.cia (~284 MB)
```

(`tools/makerom` + `tools/bannertool` are vendored; the `cia` target also has a
`tools-fetch` fallback that downloads them if missing.)

### 2. Push over FTP (ftpd-pro: port 5000, anonymous, `/cia/`)

```
# confirm the console is up first
timeout 6 bash -c "</dev/tcp/<console-ip>/5000" && echo OPEN
curl -sS -T build/3ds/forsaken3ds.cia ftp://<console-ip>:5000/cia/
# verify byte-exact on device:
curl -s ftp://<console-ip>:5000/cia/ | grep forsaken     # size must match local
```

A 284 MB push takes ~6 min over the LAN. `curl` rc=0 + matching size on the
device listing = delivered.

### 3. Install + launch

Open the CIA in **FBI → Install** (updates the existing Forsaken title), then
launch from HOME.

---

## What each path can verify (be honest in reports)

- **Headless stub (A):** boot/init/first-frame/no-crash and frame rate only. It
  **cannot** drive buttons/sticks, so keyboard triggers, stick routing, and
  overlay edits are **not** verifiable here.
- **CIA (B):** the functional path. C-stick rows still need **New 3DS or a
  Circle Pad Pro** — an OG without CPP cannot exercise them.
- Never claim a feature "tested on hardware" from a path that can't exercise it.

## Cleanup

- Remove `/tmp/kilo/*.3dsx` test artifacts.
- Stub appliance: leave it `reboot`-restored and `status` = listening.
- Do not leave a console mid-transfer; reboot the stub after Workflow A.

## Hard rules (from netload_stub/AGENTS.md)

- Only `atmctl.py` for device ops; **never** `atm run`/`atm launch` on a live
  stub. Delivery to the stub is **3dslink only**.
- Always pass `--device-id` (fail-closed identity).
- One HTTP/transfer client at a time; never stack concurrent clients.
- Cycle reset is a **remote reboot** (`terminate` is ineffective on a GPU-blocked app).
