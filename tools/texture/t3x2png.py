#!/usr/bin/env python3
"""t3x2png - recover PNG masters from .t3x texture dumps.

Input is the raw level-0 dump produced by the t3xconv homebrew
(sdmc:/t3xconv/out/<name>.<W>x<H>.f<fmt>.raw): ETC1 (fmt 12) or
ETC1A4 (fmt 13) data in 3DS GPU texture layout, as confirmed
against tex3ds ground-truth encodes:

  - the image is tiled in 2x2 ETC-block tiles (8x8 texels), tiles
    in raster order, blocks raster order inside a tile;
  - ETC1A4 is interleaved per block: 8 alpha bytes, then the
    8-byte ETC1 block;
  - alpha nibbles are column-major within the 4x4 block
    (index = x*4 + y, high nibble first);
  - ETC1 blocks are stored byte-reversed (little-endian u64)
    relative to the Khronos ETC1 layout.

These are recovery masters: pixel content is exactly what the
shipped .t3x encodes, no better. Prefer original sources when
they exist (see regen_hd_textures_lomem.sh source priority).
"""
import sys
from PIL import Image

MOD = [(2, 8), (5, 17), (9, 29), (13, 42), (18, 60), (24, 80), (33, 106), (47, 183)]


def ext4(v):
    return (v << 4) | v


def ext5(v):
    return (v << 3) | (v >> 2)


def ext3s(v):
    v &= 7
    return v - 8 if v > 3 else v


def decode_block(raw):
    """Decode one 3DS-stored ETC1 block to 16 (r,g,b), raster order.

    Storage conventions (derived empirically against tex3ds
    ground-truth encodes, see tools/texture/test_t3x2png.py):
      - logical block bytes are the raw bytes permuted
        (7,6,5,4,2,3,0,1): bases+flags from the reversed second
        word, index planes from the first word's byte pairs;
      - the flag byte carries diff in bit 1 and flip in bit 0
        (swapped relative to the Khronos ETC1 layout);
      - texels are column-major, index bits are read LSB-first,
        and the modifier order is reversed (+b,+a,-a,-b).
    """
    blk = bytes(raw[p] for p in (7, 6, 5, 4, 2, 3, 0, 1))
    diff = (blk[3] >> 1) & 1
    flip = blk[3] & 1
    t0 = (blk[3] >> 5) & 7
    t1 = (blk[3] >> 2) & 7
    if diff:
        cols = []
        for s in range(2):
            col = []
            for c in range(3):
                b = blk[c] >> 3
                if s:
                    b = max(0, min(31, b + ext3s(blk[c] & 7)))
                col.append(ext5(b))
            cols.append(tuple(col))
    else:
        cols = [tuple(ext4((blk[c] >> 4) & 15) for c in range(3)),
                tuple(ext4(blk[c] & 15) for c in range(3))]
    msb = (blk[4] << 8) | blk[5]
    lsb = (blk[6] << 8) | blk[7]
    px = [(0, 0, 0)] * 16
    for i in range(16):
        x, y = i // 4, i % 4  # column-major texel order
        mb = (msb >> i) & 1
        lb = (lsb >> i) & 1
        idx = (mb << 1) | lb
        region = 0 if ((y < 2) if flip else (x < 2)) else 1
        a, b = MOD[t0 if region == 0 else t1]
        mv = [b, a, -a, -b][idx]
        col = cols[region]
        px[y * 4 + x] = tuple(max(0, min(255, col[c] + mv)) for c in range(3))
    return px


def main():
    inp, outp = sys.argv[1], sys.argv[2]
    parts = inp.rsplit(".", 3)
    dims, fmt = parts[-3], int(parts[-2][1:])
    w, h = (int(v) for v in dims.split("x"))
    data = open(inp, "rb").read()

    img = Image.new("RGBA", (w, h))
    px = img.load()
    bw, bh = w // 4, h // 4
    tiles_x = bw // 2
    unit = 16 if fmt == 13 else 8

    for by in range(bh):
        for bx in range(bw):
            tile = (by // 2) * tiles_x + (bx // 2)
            within = (by % 2) * 2 + (bx % 2)
            off = (tile * 4 + within) * unit
            if fmt == 13:
                ablk = data[off:off + 8]
                rblk = data[off + 8:off + 16]
            else:
                ablk = None
                rblk = data[off:off + 8]
            dec = decode_block(rblk)
            for y in range(4):
                for x in range(4):
                    n = x * 4 + y  # column-major within block
                    a = 255
                    if ablk is not None:
                        byte = ablk[n // 2]
                        nib = (byte >> 4) & 15 if n % 2 == 0 else byte & 15
                        a = nib * 17
                    r, g, b = dec[y * 4 + x]
                    px[bx * 4 + x, by * 4 + y] = (r, g, b, a)
    # Texture data is bottom-up (row 0 = image bottom); PNG masters
    # are top-down, matching every other master in data/.
    img = img.transpose(Image.FLIP_TOP_BOTTOM)
    img.save(outp)
    print(f"{inp}: {w}x{h} fmt={fmt} -> {outp}")


if __name__ == "__main__":
    main()
