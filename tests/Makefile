# Host unit tests for the Forsaken 3DS tree (Linux/gcc, no console needed).
#
#   make            — build + run all host tests
#   make test_secwalk — the SecBull used-chain bound regression test
#
# test_secwalk links the REAL ailib.c against stubbed data-layer deps and
# proves AI_GetNearestMINETarget terminates on a corrupt/empty Prev chain
# (BSS-zero state: FirstSecBullUsed == 0 with all Prev == 0). Pre-fix this
# hangs (observe with `timeout 5`); post-fix all three cases exit 0.

CC      := gcc
INCLUDE := -I.. -I/usr/include/SDL
CFLAGS  := -g -fcommon -Wall -Wextra $(INCLUDE)

AILIB_OBJ := secwalk_ailib.o

all: test_secwalk test_framedump test_checkhitenemy test_stereomemo test_modules test_quat test_sphere

$(AILIB_OBJ): ../ailib.c
	$(CC) $(CFLAGS) -c -o $@ $<

test_secwalk: test_secwalk.c $(AILIB_OBJ)
	$(CC) $(CFLAGS) -o $@ test_secwalk.c $(AILIB_OBJ) -lm
	./test_secwalk

test_framedump: test_framedump.c ../framedump_core.c
	$(CC) $(CFLAGS) -DFRAMEDUMP_HOSTTEST -o $@ test_framedump.c ../framedump_core.c
	./test_framedump

test_checkhitenemy: test_checkhitenemy.c ../checkenemy.c ../sphere.c
	$(CC) $(CFLAGS) -DCHE_PREFILTER -o $@ test_checkhitenemy.c ../checkenemy.c ../sphere.c -lm
	./test_checkhitenemy

test_stereomemo: test_stereomemo.c ../stereomemo.c
	$(CC) $(CFLAGS) -DSTEREO_REPLAY -o $@ test_stereomemo.c ../stereomemo.c
	./test_stereomemo

test_modules: test_modules.cpp ../dsp_adpcm.cpp ../hunk_alloc.cpp
	g++ -g -Wall -Wextra $(INCLUDE) -std=gnu++17 -fno-exceptions -fno-rtti -o $@ test_modules.cpp ../dsp_adpcm.cpp ../hunk_alloc.cpp
	./test_modules

test_quat: test_quat.cpp ../quat.cpp ../new3d.c
	g++ -g -Wall -Wextra $(INCLUDE) -std=gnu++17 -fno-exceptions -fno-rtti -o $@ test_quat.cpp ../quat.cpp ../new3d.c -lm
	./test_quat

test_sphere: test_sphere.cpp ../sphere.cpp ../new3d.c
	g++ -g -Wall -Wextra $(INCLUDE) -std=gnu++17 -fno-exceptions -fno-rtti -o $@ test_sphere.cpp ../sphere.cpp ../new3d.c -lm
	./test_sphere
	./test_quat

clean:
	rm -f test_secwalk test_framedump test_checkhitenemy test_stereomemo test_modules test_quat test_sphere $(AILIB_OBJ) out-*.bmp framedump_test.log

.PHONY: all clean test_secwalk test_framedump test_checkhitenemy test_stereomemo test_modules test_quat test_sphere
