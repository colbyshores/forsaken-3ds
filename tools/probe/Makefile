# Standalone Makefile for the Forsaken 3DS Probe.
#
# Targets:
#   make            -> probe.cia (Application mode, what we need)
#   make clean
#
# Only the CIA is useful here: a .3dsx would launch under HBL's applet
# mode (~32 MB, totally different budget than Application mode), so it
# can't probe what we need to know.  CIA-only build keeps things simple.

ifeq ($(strip $(DEVKITPRO)),)
$(error Please set DEVKITPRO in your environment.)
endif
ifeq ($(strip $(DEVKITARM)),)
$(error Please set DEVKITARM in your environment.)
endif

include $(DEVKITARM)/3ds_rules

TARGET   := probe
SOURCES  := probe.c
BUILD    := build

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := $(ARCH) -O2 -Wall -Wextra -std=gnu99 \
            -ffunction-sections -fdata-sections \
            -D__3DS__ -I$(DEVKITPRO)/libctru/include
LDFLAGS  := $(ARCH) -specs=3dsx.specs -Wl,-Map,$(TARGET).map \
            -L$(DEVKITPRO)/libctru/lib
LIBS     := -lctru -lm

CC       := arm-none-eabi-gcc

# SMDH metadata
APP_TITLE       := Forsaken Probe
APP_DESCRIPTION := Memory budget probe for OG 3DS diagnostics
APP_AUTHOR      := Forsaken3DS
APP_ICON        := $(CURDIR)/../../assets/icon.png

# CIA build needs makerom, banner, RSF
MAKEROM := $(CURDIR)/../makerom

OBJS := $(BUILD)/$(SOURCES:.c=.o)

.PHONY: all clean cia

all: $(TARGET).cia

$(TARGET).elf: $(OBJS)
	@echo "Linking $(notdir $@)..."
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	@echo "$(notdir $<)"
	$(CC) -MMD -MP -MF $(BUILD)/$*.d $(CFLAGS) -c $< -o $@

$(TARGET).smdh:
	@echo "Creating SMDH..."
	smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $(APP_ICON) $@

# CIA build: uses the existing forsaken.bnr (reuse the banner from the
# main project — no point making a new one for a one-off probe) and a
# minimal RSF that requests Application mode (no HIMEM grant) with a
# distinct UniqueId so it can coexist with the main Forsaken installs.
$(TARGET).cia: $(TARGET).elf $(TARGET).smdh probe.rsf | check-banner
	@echo "Building probe CIA..."
	$(MAKEROM) -f cia -o $@ \
		-rsf probe.rsf \
		-target t -exefslogo \
		-elf $(TARGET).elf \
		-icon $(TARGET).smdh \
		-banner ../../build/3ds/forsaken3ds.bnr

.PHONY: check-banner
check-banner:
	@test -f ../../build/3ds/forsaken3ds.bnr || { \
		echo "ERROR: ../../build/3ds/forsaken3ds.bnr missing."; \
		echo "Run 'make -f Makefile.3ds cia' from the project root first (it builds the banner)."; \
		exit 1; \
	}

clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).smdh \
	        $(TARGET).cia $(TARGET).map
