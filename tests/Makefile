# Host unit tests for the Forsaken 3DS tree (Linux/gcc, no console needed).
#
#   make            — build + run all host tests
#   make test_secwalk — the SecBull used-chain bound regression test
#
# test_secwalk links the REAL ailib.cpp against stubbed data-layer deps and
# proves AI_GetNearestMINETarget terminates on a corrupt/empty Prev chain
# (BSS-zero state: FirstSecBullUsed == 0 with all Prev == 0). Pre-fix this
# hangs (observe with `timeout 5`); post-fix all three cases exit 0.
#
# Engine sources are C++ (C-with-classes), so every test builds with g++
# against the .cpp sources.

CC      := gcc
CXX     := g++
CORE    := ../forsaken_core/src
INCLUDE := -I$(CORE) $(shell pkg-config --cflags sdl2)
CXXFLAGS := -g -Wall -Wextra $(INCLUDE) -std=gnu++17 -fno-exceptions -fno-rtti

all: test_secwalk test_checkhitenemy test_stereomemo test_modules test_quat test_sphere test_new3d test_sincos test_clipmath

test_secwalk: test_secwalk.cpp $(CORE)/ai/ailib.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_secwalk.cpp $(CORE)/ai/ailib.cpp -lm
	./test_secwalk

test_checkhitenemy: test_checkhitenemy.cpp $(CORE)/game/checkenemy.cpp $(CORE)/math/sphere.cpp
	$(CXX) $(CXXFLAGS) -DCHE_PREFILTER -o $@ test_checkhitenemy.cpp $(CORE)/game/checkenemy.cpp $(CORE)/math/sphere.cpp -lm
	./test_checkhitenemy

test_stereomemo: test_stereomemo.c $(CORE)/rendering/stereomemo.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_stereomemo.c $(CORE)/rendering/stereomemo.cpp
	./test_stereomemo

test_modules: test_modules.cpp $(CORE)/audio/dsp_adpcm.cpp $(CORE)/util/hunk_alloc.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_modules.cpp $(CORE)/audio/dsp_adpcm.cpp $(CORE)/util/hunk_alloc.cpp
	./test_modules

test_quat: test_quat.cpp $(CORE)/math/quat.cpp $(CORE)/math/new3d.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_quat.cpp $(CORE)/math/quat.cpp $(CORE)/math/new3d.cpp -lm
	./test_quat

test_sphere: test_sphere.cpp $(CORE)/math/sphere.cpp $(CORE)/math/new3d.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_sphere.cpp $(CORE)/math/sphere.cpp $(CORE)/math/new3d.cpp -lm
	./test_sphere

test_new3d: test_new3d.cpp $(CORE)/math/new3d.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_new3d.cpp $(CORE)/math/new3d.cpp -lm
	./test_new3d

test_sincos: test_sincos.cpp $(CORE)/math/new3d.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_sincos.cpp $(CORE)/math/new3d.cpp -lm
	./test_sincos

test_clipmath: test_clipmath.cpp $(CORE)/world/clipmath.h
	$(CXX) $(CXXFLAGS) -o $@ test_clipmath.cpp -lm
	./test_clipmath

clean:
	rm -f test_secwalk test_checkhitenemy test_stereomemo test_modules test_quat test_sphere test_new3d test_sincos test_clipmath

.PHONY: all clean test_secwalk test_checkhitenemy test_stereomemo test_modules test_quat test_sphere test_new3d test_sincos test_clipmath
