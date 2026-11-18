# Mipmap pipeline — proof it exists in the Forsaken 3DS port

Reference document for cross-port work (e.g. a Perfect Dark 3DS port using
this repo as a template). Every claim below is backed by a commit hash on
`master` and a current `file:line`. Verification commands at the bottom can
be run from the repo root to reproduce all of it.

Short version: **yes, the mipmap chain is implemented and wired end to end** —
generated at asset-build time with `tex3ds -m gaussian`, shipped inside the
`.t3x` files, loaded whole at runtime by `Tex3DS_TextureImportStdio`, and
sampled with a bilinear-mipmap filter (`C3D_TexSetFilterMipmap(GPU_NEAREST)`).
It is NOT trilinear, and textures that fall through to the runtime
BMP→RGBA4 path are single-level by design. Details below.

---

## 1. Build side — mip chains are generated into the `.t3x`

The HD texture regenerators encode every texture with `tex3ds -m gaussian`,
which is the flag that tells `tex3ds` to build a mipmap chain (Gaussian
downsample filter) and append it to the `.t3x`.

- `regen_hd_textures_4k.sh:233,235,237` (512² N3DS / HIMEM pack)
- `regen_hd_textures_lomem.sh:122,124` (256² OG pack)

```sh
# regen_hd_textures_4k.sh — walls/sprites/UI all Gaussian-mipped
tex3ds -f etc1      -q high -m gaussian "$tmp_png" -o "$dst"   # opaque walls (ETC1)
tex3ds -f auto-etc1 -q high -m gaussian "$tmp_png" -o "$dst"   # alpha walls (ETC1A4)
tex3ds -f auto-etc1 -q high -m gaussian "$tmp_png" -o "$dst"   # everything else
```

### Commit history (all on `master`)

| Commit | Date | What it did |
|---|---|---|
| `edbc89ed0dc44793615cbb317b614e22e223accf` | 2026-04-17 | Add HD texture support using ETC1A4 compression (`.t3x` pipeline introduced) |
| `fa276a986e7e6a7ebb41390ca1eea1ae798296cb` | 2026-04-18 | HD texture pipeline; **adds `C3D_TexSetFilterMipmap` at the load site** (runtime mip sampling turned on) |
| `89518a2027601796930651f40e755c9ef61f8c12` | 2026-04-24 | regen: **switch ETC1 mipmap filter from bilinear to gaussian** |
| `4c3dbc2cacd61732e6ab9dad8a25235d1063fa6a` | 2026-04-24 | regen: **Gaussian-mip every wall**, drop the `skip_mips` and alpha-wall mip carve-outs |
| `e4feb9ab49fcf35bdbb6db82d660ee6f1288a05c` | 2026-05-04 | Drop OG-3DS mip-0 strip — full 512² + full chain fits post-Q3 refactor |

Confirm any of them is on master:
```sh
git merge-base --is-ancestor 89518a2 master && echo "on master"
git show -s --format='%H %ad %s' --date=short 4c3dbc2
```

### Empirical proof the chain is actually in the file (not just the flag)

Re-encoding the same 256² source with and without `-m gaussian`:

```
no -m        : 10401 bytes   (base level only)
-m gaussian  : 15265 bytes   (+47% — the appended mip levels)
```

A 47% size increase from a filter flag is the mip chain; a pure base-level
re-filter would not change the encoded size like that. The shipped
512²-source `.t3x` (e.g. `hd_textures/textures/flshmrph.t3x`) is ~60 KB,
consistent with a 512² base + full chain.

---

## 2. Runtime side — the chain is loaded and sampled

`render_c3d.c`, function that loads an HD texture (`try_load_hd_texture`):

- `render_c3d.c:2333` — `Tex3DS_TextureImportStdio` loads the texture
  *including every mip level present in the `.t3x`*. libctru sets the
  texture's `maxLevel` from the chain in the file.
- `render_c3d.c:2352-2353` — filter setup:

```c
/* render_c3d.c:2348 (comment) + 2352-2353 (code) */
/* Bilinear-mipmap (GPU_NEAREST for mip selection): picks ONE mip level
 * per fragment, then bilinear-filters within it. Gives anti-moiré
 * without the trilinear softness or the doubled per-fragment texture
 * taps. If the t3x has no mip chain, GPU clamps to mip 0. */
C3D_TexSetFilter(&texdata->tex, GPU_LINEAR, GPU_LINEAR);   /* min/mag within a level */
C3D_TexSetFilterMipmap(&texdata->tex, GPU_NEAREST);        /* <-- mip-LEVEL selection */
```

`C3D_TexSetFilterMipmap(..., GPU_NEAREST)` is the line that turns mip
sampling on. Without it the GPU would clamp to level 0 regardless of what
the `.t3x` contains. With it, PICA200 picks the appropriate mip per fragment
based on screen-space derivatives.

This call site was introduced in `fa276a9` (the `git log -L` for these lines
shows it being added there, replacing the earlier no-mipmap-filter version
from `edbc89e`).

---

## 3. Two honest caveats (so the claim is precise, not overstated)

1. **Bilinear-mipmap, not trilinear.** `GPU_NEAREST` for the *mip level
   selector* means the GPU snaps to one level then bilinear-filters within
   it — no blending *between* levels. Deliberate: avoids trilinear softness
   and the doubled per-fragment texture taps on a 268 MHz PICA200. The
   visible trade-off is an occasional mip *seam* where the level switches,
   instead of a smooth cross-fade. (Comment at `render_c3d.c:2348`.)

2. **Textures with no HD `.t3x` companion get no mips.** The fallback path
   that converts a BMP/PNG to RGBA4 at runtime allocates a single level:

   ```c
   /* render_c3d.c:2472 — create_texture fallback, single level only */
   C3D_TexInit(&texdata->tex, image.w, image.h, GPU_RGBA4);
   ...
   C3D_TexSetFilter(&texdata->tex, GPU_LINEAR, GPU_LINEAR);  /* no SetFilterMipmap */
   ```

   There is no `C3D_TexSetFilterMipmap` here and `C3D_TexInit` allocates no
   mip storage, so these are mip-less by construction. Any surface whose
   texture isn't in the HD `.t3x` pack runs bilinear-only and can shimmer at
   distance. That is a coverage gap, NOT a "mips aren't implemented" gap.

---

## 4. One-shot verification (run from repo root)

```sh
# (a) build side: the gaussian-mip flag is in the regenerators
grep -n 'tex3ds .*-m gaussian' regen_hd_textures_4k.sh regen_hd_textures_lomem.sh

# (b) runtime side: the mip-selection filter call exists
grep -n 'C3D_TexSetFilterMipmap' render_c3d.c
grep -n 'Tex3DS_TextureImportStdio' render_c3d.c

# (c) commits are real and on master
for h in edbc89e fa276a9 89518a2 4c3dbc2 e4feb9a; do
  git show -s --format='%H %ad %s' --date=short $h
  git merge-base --is-ancestor $h master && echo "   -> on master"
done

# (d) the .t3x files actually carry chains (size jump from -m gaussian)
tex3ds -f auto-etc1 -q high            Data/textures/flshmrph.bmp -o /tmp/nomip.t3x
tex3ds -f auto-etc1 -q high -m gaussian Data/textures/flshmrph.bmp -o /tmp/gauss.t3x
stat -c '%n %s bytes' /tmp/nomip.t3x /tmp/gauss.t3x   # gauss is ~47% larger
```

If another tool/agent claims "the mipmap chain isn't here," (a) and (b)
above are the two-line refutation: the generator passes `-m gaussian`, and
the loader calls `C3D_TexSetFilterMipmap(GPU_NEAREST)` right after
`Tex3DS_TextureImportStdio`. Both are on `master`.
