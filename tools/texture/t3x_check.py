"""t3x_check - validate .t3x headers before they ship.

The declared (decompressed) data size must equal what the format, size and
mip count require; an uncompressed file must also hold exactly that many
bytes. Tex3DS_TextureImport trusts the header: a short size field imports
only that many bytes and leaves the rest of the texture as whatever was in
memory (2026-11-17: hd_textures/levels/vol2/textures/thermd.t3x declared
1024 of 262144 bytes and drew another image on the volcano lava floor).

Header layout (libctru tex3ds.c): u16 numSubTextures; u8 {width_log2-3:3,
height_log2-3:3, type:1}; u8 format; u8 mipmapLevels; 12 bytes per
subtexture; then a 4-byte compression header (type, 24-bit size; size 0 =>
32-bit size follows).

Usage: tools/texture/t3x_check.py DIR... (exit 1 if any file is bad)"""
import os, sys
BPP = {0x0: 32, 0x1: 24, 0x2: 16, 0x3: 16, 0x4: 16, 0x5: 16, 0x6: 16, 0x7: 8, 0x8: 8,
       0x9: 8, 0xA: 4, 0xB: 4, 0xC: 4, 0xD: 8}       # GPU_TEXCOLOR bits per texel
bad = 0; n = 0
for root in sys.argv[1:]:
    for d, _, fs in os.walk(root):
        for f in fs:
            if not f.endswith(".t3x"): continue
            p = os.path.join(d, f); b = open(p, "rb").read(); n += 1
            nsub = b[0] | b[1] << 8
            wl, hl = (b[2] & 7) + 3, ((b[2] >> 3) & 7) + 3
            fmt, mips = b[3], b[4]
            off = 5 + 12 * nsub
            ctype = b[off]; size = b[off + 1] | b[off + 2] << 8 | b[off + 3] << 16
            hdr = 4
            if size == 0 and ctype != 0:      # extended size field
                size = int.from_bytes(b[off + 4:off + 8], "little"); hdr = 8
            w, h = 1 << wl, 1 << hl
            want = sum(((w >> i) * (h >> i) * BPP.get(fmt, 0)) // 8 for i in range(mips + 1))
            avail = len(b) - off - hdr
            ok = size == want and (ctype != 0 or avail == size)
            if not ok:
                bad += 1
                print(f"BAD {p}: {w}x{h} fmt={fmt:#x} mips={mips} ctype={ctype:#x} "
                      f"declared={size} expected={want} file_payload={avail}")
print(f"checked {n}, bad {bad}")
sys.exit(1 if bad else 0)
