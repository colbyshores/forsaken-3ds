# Level orderings

The single source of truth for level lists on both builds. `Makefile.3ds`
copies the edition's lists into the romfs staging; the PC `Makefile`
installs them into `Data/levels/`. Either way they replace whatever
`extract_assets.py` / `extract_remaster_levels.py` produced.

| File | Edition | Purpose |
|------|---------|---------|
| `mission.dat` | remaster | Single-player campaign order: the Remaster's SP sequence plus the N64-port secret levels (30 entries; `biolab` and `tuben64` are out of the rotation). |
| `battle.dat` | remaster | Multiplayer arena list: the Remaster's curated MP-classified levels. |
| `original/mission.dat` | original | The authoritative 15-level 1998 SP campaign order, taken from `Forsaken.exe`. |

The Remaster orderings come from the Remaster's `defs/mapInfo.txt`
(SP_Level / MP_Level entries).

Music is keyed by level name (`forsaken_core/src/audio/music_table.cpp`),
not by position in these lists, so adding, dropping or reordering a level
needs no music change.
