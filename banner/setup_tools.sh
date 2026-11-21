#!/usr/bin/env bash
# One-time setup for the animated HOME-menu banner pipeline (see SPEC.md).
# Clones pycgfx at a pinned commit and makes a venv with its dependencies,
# both under banner/.tools/ (not tracked).
set -euo pipefail
cd "$(dirname "$0")"
PYCGFX_COMMIT=6c2e0cd53a5f4c3265b9aa9a1f30806f8291116e   # = 1f78850 (hardware-tested) + MIT LICENSE
mkdir -p .tools
if [ ! -d .tools/pycgfx/.git ]; then
	git clone -q https://github.com/skyfloogle/pycgfx.git .tools/pycgfx
fi
git -C .tools/pycgfx fetch -q origin
git -C .tools/pycgfx checkout -q "$PYCGFX_COMMIT"
if [ ! -x .tools/venv/bin/python ]; then
	python3 -m venv .tools/venv
fi
.tools/venv/bin/pip install -q gltflib pillow numpy
echo "banner tools ready: .tools/pycgfx @ ${PYCGFX_COMMIT:0:7}, .tools/venv"
