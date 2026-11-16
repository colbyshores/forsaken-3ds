#
# General Compiler Settings
#

# for cross compiling be sure to specify your compiler ex:
# make CC=i686-mingw32-gcc
CC=gcc

# general compiler settings
ifeq ($(M32),1)
  FLAGS+= -m32
endif
FLAGS+= -std=gnu99 -pipe -fcommon
CFLAGS=$(FLAGS) -Wall -Wextra
LDFLAGS=$(FLAGS)

# right now non debug build would probably crash anyway
# we even release debug builds as the official release
DEBUG=1

# might as well leave gprof support on by default as well
PROFILE=1

# stack-smash protection against buffer overflows and corrupt pointers
# (enabled by default on many systems today)
ifeq ($(SSP),1)
  FLAGS+= -fstack-protector-all
  CFLAGS+= -D_FORTIFY_SOURCE=2
endif

ifeq ($(DEBUG),1)
  FLAGS+= -g
else
  CFLAGS+=-O3 -Winit-self
  LDFLAGS+=-s
endif

ifeq ($(PROFILE),1)
  ifneq ($(DEBUG),1)
    # Debug symbols needed for profiling to be useful
    FLAGS+= -g
  endif
  FLAGS+= -pg
endif

#
# ProjectX Specific
#

# some systems use lua5.1
LUA=$(shell pkg-config --exists lua5.1 && echo lua5.1 || echo lua)

# which version of sdl do you want to ask pkgconfig for ?
SDL=1
ifeq ($(SDL),1)
  SDL_=sdl
else
  SDL_=sdl$(SDL)
endif

# which version of GL do you want to use ?
GL=1

$(if $(shell test "$(GL)" -ge 3 -a "$(SDL)" -lt 2 && echo fail), \
     $(error "GL >= 3 only supported with SDL >= 2"))

# library headers
CFLAGS+= `pkg-config --cflags $(SDL_) $(LUA) $(LUA)-socket libenet libpng zlib openal`

# libraries (Linux host only)
LIB+= `pkg-config --libs $(LUA) $(LUA)-socket libenet libpng zlib openal` -lm
LIB+= `pkg-config --libs $(SDL_)`
LIB+= -lGL -lGLU -ldl

# ProjectX-specific includes
CFLAGS += -I.

# ProjectX-specific macros
  CFLAGS+= -DGL=$(GL)
CFLAGS+= -DNET_ENET_2 -DBSP -DLUA_USE_APICHECK -DTEXTURE_PNG -DSOUND_SUPPORT -DSOUND_OPENAL
ifeq ($(DEBUG),1)
  CFLAGS+= -DDEBUG_ON -DDEBUG_COMP -DDEBUG_SPOTFX_SOUND -DDEBUG_VIEWPORT
endif




INC=$(wildcard *.h)
SRC=$(wildcard *.c)
OBJ=$(patsubst %.c,%.o,$(SRC))

# allows user to override settings
ADD_FLAGS=
ADD_CFLAGS=
ADD_LDFLAGS=
FLAGS+=$(ADD_FLAGS)
CFLAGS+=$(ADD_CFLAGS)
LDFLAGS+=$(ADD_LDFLAGS)

# Content edition, same convention as Makefile.3ds: EDITION=remaster
# (default) defines EDITION_REMASTER; EDITION=original defines nothing.
EDITION ?= remaster
ifeq ($(EDITION),remaster)
  CFLAGS+= -DEDITION_REMASTER
endif

BIN=projectx

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) -o $(BIN) $(OBJ) $(LDFLAGS) $(LIB)

$(OBJ): $(INC)

clean:
	$(RM) $(OBJ) $(BIN)

check:
	@echo
	@echo "INC = $(INC)"
	@echo
	@echo "SRC = $(SRC)"
	@echo
	@echo "OBJ = $(OBJ)"
	@echo
	@echo "DEBUG = $(DEBUG)"
	@echo "PROFILE = $(PROFILE)"
	@echo "PKG_CFG_OPTS = $(PKG_CFG_OPTS)"
	@echo "GL = $(GL)"
	@echo "LUA = $(LUA)"
	@echo "SDL = $(SDL)"
	@echo "SDL_ = $(SDL_)"
	@echo
	@echo "CC = $(CC)"
	@echo "BIN = $(BIN)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "LIB = $(LIB)"
	@echo

.PHONY: all clean
