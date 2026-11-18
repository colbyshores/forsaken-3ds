#!/usr/bin/env python3
"""png_t3x_twins.py ROMFS_DIR -- list the PNGs the 3DS never opens.

The 3DS texture loader (render_c3d.cpp create_texture) loads the HD .t3x
for a texture path when either pack (hd_textures/, hd_textures_lomem/)
has one, and only falls back to the PNG when neither does; tload's path
choice asks FSTextureExists, which accepts the .t3x too. So a PNG under
data/ whose lowercased path has a .t3x in either pack is never read.

Prints one rsync exclude pattern per such PNG ("/data/<path as spelled in
ROMFS_DIR>"), for Makefile.3ds to leave them out of the romfs.
"""
import os
import sys


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    romfs = sys.argv[1]

    keys = set()
    for pack in ('hd_textures', 'hd_textures_lomem'):
        top = os.path.join(romfs, pack)
        for root, _dirs, files in os.walk(top, followlinks=True):
            for name in files:
                if name.lower().endswith('.t3x'):
                    rel = os.path.relpath(os.path.join(root, name), top)
                    keys.add(rel.lower()[:-len('.t3x')])

    data = os.path.join(romfs, 'data')
    for root, _dirs, files in os.walk(data, followlinks=True):
        for name in sorted(files):
            if not name.lower().endswith('.png'):
                continue
            rel = os.path.relpath(os.path.join(root, name), data)
            if rel.lower()[:-len('.png')] in keys:
                print('/data/' + rel.replace(os.sep, '/'))


if __name__ == '__main__':
    main()
