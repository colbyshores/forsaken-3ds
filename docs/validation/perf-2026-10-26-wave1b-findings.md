# Wave-1b analysis — cam breakdown, per-frame (stereo ON: 2 camera calls/frame)

Raw: perf-2026-10-26-wave1b.log (rows with stage=7..10). Sub-stage values in
the auto-table are PER-CALL; per-frame = value x 2 (stereo). Corrected table
(ms/frame, worst frame excluded):

level        fps   cam  | vis   bg  opaque trans | HUD+wrapper (residual)
powerdown   45.1  9.2  | 1.0  4.4   2.0   0.2  | ~1.6
ramqan      36.2  9.2  | 1.3  2.2   1.8   2.0  | ~1.9
vol2        55.7  2.2  | 0.2  0.6   0.2   0.0  | ~1.2
battlebase  53.4  1.2  | 0.2  0.2   0.0   0.2  | ~0.6

Findings:
1. DisplayBackground hypothesis CONFIRMed for powerdown: bg region = 4.4
   ms/frame (half its cam cost). NOT the story for ramqan (2.2).
2. ramqan cost is spread: trans 2.0 + bg 2.2 + opaque 1.8 + vis 1.3 — no
   single hotspot; translucent pass is notable (heavy alpha geometry).
3. Constant residual ~1.2-2.0 ms/frame across ALL levels = per-eye HUD
   overlay (DrawMainGameMenu x2 eyes) + stereo wrapper. Affects every
   level equally; ~1.5 ms/frame is a universal win if the HUD can be
   drawn once per frame instead of per-eye.
4. Stereo confirmed ON in autotest config (sub-stage counts = 2.1x frames).
   All cam numbers are for the stereo path; mono would roughly halve them.

Optimization targets from wave 1:
- A) powerdown: DisplayBackground path (bg region 4.4 ms/frame)
- B) all levels: per-eye HUD overlay dedupe (~1.5 ms/frame universal)
- C) ramqan: translucent pass review (2.0 ms/frame) — lower priority,
  spread cost means no single fix.
