# Level orderings

Level lists for the `EDITION=remaster` build flavour. The Makefile copies
these into the romfs staging at build time, replacing the 1998 originals
that come from `extract_assets.py`. See the top-level README for the
build workflow.

| File          | Purpose                                                          |
|---------------|------------------------------------------------------------------|
| `mission.dat` | Single-player campaign order — Remaster's authoritative 24-entry sequence plus the 8 N64-port secret levels (32 entries total). Indices line up with `s_level_track[]` under `#ifdef EDITION_REMASTER` in `music_3ds.c`. |
| `battle.dat`  | Multiplayer arena list — Remaster's curated 24 MP-classified levels (the original 30-entry 1998 list is retained for the default build). |

These orderings come straight from the Remaster's `defs/mapInfo.txt`
(SP_Level / MP_Level entries), preserving Night Dive's intended
campaign sequence.
