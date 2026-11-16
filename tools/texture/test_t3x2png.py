#!/usr/bin/env python3
"""Regression test for t3x2png's ETC1 block decode.

The five blocks below are real tex3ds-encoded diagnostic blocks
(extracted via the t3xconv homebrew from a synthetic 64x64 image
with known per-block content). They pin the 3DS ETC1 storage
conventions: byte permutation, swapped diff/flip flag bits,
column-major texels, LSB-first index bits, reversed modifiers.
If any of those regress, this test fails before the masters do.

Run: python3 tools/texture/test_t3x2png.py
"""
import importlib.util
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("t3x2png", os.path.join(HERE, "t3x2png.py"))
mod = importlib.util.module_from_spec(spec)
# t3x2png only runs main() under __main__, safe to import
spec.loader.exec_module(mod)

CASES = [
    # horizontal red ramp (0, 85, 170, 255 per column)
    ("0f0fffffd422226f", [(x * 85, 0, 0) for y in range(4) for x in range(4)]),
    # vertical green ramp
    ("5555ffffd5226f22", [(0, y * 85, 0) for y in range(4) for x in range(4)]),
    # flat (10, 20, 30)
    ("0000ffff6e282018", [(10, 20, 30)] * 16),
    # left red / right blue
    ("0000ffff000f00f0", [(255, 0, 0) if x < 2 else (0, 0, 255) for y in range(4) for x in range(4)]),
    # top yellow / bottom cyan
    ("00000000010ffff0", [(255, 255, 0) if y < 2 else (0, 255, 255) for y in range(4) for x in range(4)]),
]

# ETC is lossy; a correct decode of these blocks stays well under
# this per-texel-per-channel budget (measured: ~16 average).
MAX_AVG_ERR = 40.0


def main():
    total = 0
    for hexs, src in CASES:
        dec = mod.decode_block(bytes.fromhex(hexs))
        err = sum(abs(dec[i][c] - src[i][c]) for i in range(16) for c in range(3))
        total += err
        print(f"block {hexs}: total err {err}")
    avg = total / (len(CASES) * 16 * 3)
    print(f"average err per texel-channel: {avg:.1f} (limit {MAX_AVG_ERR})")
    if avg > MAX_AVG_ERR:
        print("FAIL: ETC decode conventions regressed")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
