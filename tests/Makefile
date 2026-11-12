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

all: test_secwalk test_framedump test_checkhitenemy test_atm_core_div test_sincos

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

# test_atm_core_div links the REAL atm_core.c (included for its static
# round-up helper) and proves the f64 R11 replacements are bit-exact vs
# integer division across the claimed input domain.
test_atm_core_div: test_atm_core_div.c ../atm_core.c atm/atm_core.h ../atm_core.h
	$(CC) $(CFLAGS) -I. -o $@ test_atm_core_div.c -lm
	./test_atm_core_div

clean:
	rm -f test_secwalk test_framedump test_checkhitenemy test_atm_core_div test_sincos $(AILIB_OBJ) out-*.bmp framedump_test.log

test_sincos: test_sincos.c ../new3d.h trigshim/main.h
	$(CC) $(CFLAGS) -Itrigshim -o $@ test_sincos.c -lm
	./test_sincos

.PHONY: all clean test_secwalk test_framedump test_checkhitenemy test_atm_core_div test_sincos
