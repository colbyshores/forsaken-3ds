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
# against the .cpp sources. test_framedump was dropped when framedump_core.c
# moved to the harness tree (ad420e3); the framedump proof chain lives there.

CC      := gcc
CXX     := g++
INCLUDE := -I.. $(shell pkg-config --cflags sdl2)
CXXFLAGS := -g -Wall -Wextra $(INCLUDE) -std=gnu++17 -fno-exceptions -fno-rtti

all: test_secwalk test_checkhitenemy test_stereomemo test_modules test_quat test_sphere test_new3d

test_secwalk: test_secwalk.cpp ../ailib.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_secwalk.cpp ../ailib.cpp -lm
	./test_secwalk

test_checkhitenemy: test_checkhitenemy.cpp ../checkenemy.cpp ../sphere.cpp
	$(CXX) $(CXXFLAGS) -DCHE_PREFILTER -o $@ test_checkhitenemy.cpp ../checkenemy.cpp ../sphere.cpp -lm
	./test_checkhitenemy

test_stereomemo: test_stereomemo.c ../stereomemo.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_stereomemo.c ../stereomemo.cpp
	./test_stereomemo

test_modules: test_modules.cpp ../dsp_adpcm.cpp ../hunk_alloc.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_modules.cpp ../dsp_adpcm.cpp ../hunk_alloc.cpp
	./test_modules

test_quat: test_quat.cpp ../quat.cpp ../new3d.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_quat.cpp ../quat.cpp ../new3d.cpp -lm
	./test_quat

test_sphere: test_sphere.cpp ../sphere.cpp ../new3d.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_sphere.cpp ../sphere.cpp ../new3d.cpp -lm
	./test_sphere

test_new3d: test_new3d.cpp ../new3d.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_new3d.cpp ../new3d.cpp -lm
	./test_new3d

clean:
	rm -f test_secwalk test_checkhitenemy test_stereomemo test_modules test_quat test_sphere test_new3d out-*.bmp framedump_test.log

.PHONY: all clean test_secwalk test_checkhitenemy test_stereomemo test_modules test_quat test_sphere test_new3d
