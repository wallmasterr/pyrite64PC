# KallistiOS Dreamcast runtime for Pyrite64
# Invoked from project build-dc/Makefile with:
#   P64_PROJECT_PATH, P64_ROM_NAME, PYRITE64_ROOT, OUTDIR (optional)

ifndef KOS_BASE
$(error KOS_BASE is not set — source KallistiOS environ.sh before building)
endif

ifndef PYRITE64_ROOT
$(error PYRITE64_ROOT is required)
endif
ifndef P64_PROJECT_PATH
$(error P64_PROJECT_PATH is required)
endif

P64_ROM_NAME ?= p64game
OUTDIR ?= $(P64_PROJECT_PATH)/build-dc

# kos-c++ (SH-4) cannot open Windows paths like C:/... — normalize under MSYS.
# Use override: make VAR=... from the editor would otherwise ignore these := lines.
cygpath_u = $(shell cygpath -u "$(1)" 2>/dev/null || echo "$(1)")
override PYRITE64_ROOT := $(patsubst %/,%,$(call cygpath_u,$(PYRITE64_ROOT)))
override P64_PROJECT_PATH := $(patsubst %/,%,$(call cygpath_u,$(P64_PROJECT_PATH)))
override OUTDIR := $(patsubst %/,%,$(call cygpath_u,$(OUTDIR)))

OBJDIR := $(OUTDIR)/obj

include $(KOS_BASE)/Makefile.rules

TARGET := $(OUTDIR)/$(P64_ROM_NAME)_dc.elf
CDI    := $(OUTDIR)/$(P64_ROM_NAME)_dc.cdi

ENGINE := $(PYRITE64_ROOT)/n64/engine
RT     := $(PYRITE64_ROOT)/n64/runtime_dc
LD_INC := $(PYRITE64_ROOT)/vendored/libdragon/include
T3D    := $(PYRITE64_ROOT)/vendored/tiny3d/src

# --- Sources (mirror runtime_pc CMakeLists) ---
ENGINE_CPP := \
  $(wildcard $(ENGINE)/src/*.cpp) \
  $(wildcard $(ENGINE)/src/vi/*.cpp) \
  $(wildcard $(ENGINE)/src/lib/*.cpp) \
  $(wildcard $(ENGINE)/src/scene/*.cpp) \
  $(wildcard $(ENGINE)/src/audio/*.cpp) \
  $(wildcard $(ENGINE)/src/assets/*.cpp) \
  $(wildcard $(ENGINE)/src/collision/*.cpp) \
  $(wildcard $(ENGINE)/src/debug/*.cpp) \
  $(wildcard $(ENGINE)/src/debug/overlay/*.cpp) \
  $(wildcard $(ENGINE)/src/scene/components/*.cpp) \
  $(wildcard $(ENGINE)/src/renderer/*.cpp) \
  $(wildcard $(ENGINE)/src/renderer/hdr/*.cpp) \
  $(wildcard $(ENGINE)/src/renderer/bigtex/*.cpp) \
  $(wildcard $(ENGINE)/src/renderer/particles/*.cpp) \
  $(wildcard $(ENGINE)/src/script/*.cpp) \
  $(wildcard $(ENGINE)/src/libdragon/*.cpp)

# Use PC host entry points (already matched by wildcards above); drop N64-only mains.
ENGINE_CPP := $(filter-out $(ENGINE)/src/main.cpp,$(ENGINE_CPP))
ENGINE_CPP := $(filter-out $(ENGINE)/src/vi/swapChain.cpp,$(ENGINE_CPP))

RT_CPP := $(RT)/main.cpp $(RT)/dc_stubs.cpp
RT_C   := $(RT)/dc_platform.c

GAME_CPP := \
  $(wildcard $(P64_PROJECT_PATH)/src/user/*.cpp) \
  $(wildcard $(P64_PROJECT_PATH)/src/user/*/*.cpp) \
  $(wildcard $(P64_PROJECT_PATH)/src/p64/*.cpp)

# Flatten object names (path → underscores) to avoid collisions
pathobj = $(OBJDIR)/$(subst /,_,$(subst :,_,$(patsubst $(PYRITE64_ROOT)/%,%,$(patsubst $(P64_PROJECT_PATH)/%,proj/%,$(1))))).o

ENGINE_OBJS := $(foreach s,$(ENGINE_CPP),$(call pathobj,$(s)))
RT_OBJS_CPP := $(foreach s,$(RT_CPP),$(call pathobj,$(s)))
RT_OBJS_C   := $(foreach s,$(RT_C),$(call pathobj,$(s)))
GAME_OBJS   := $(foreach s,$(GAME_CPP),$(call pathobj,$(s)))
OBJS := $(ENGINE_OBJS) $(RT_OBJS_CPP) $(RT_OBJS_C) $(GAME_OBJS)

COMMON_FLAGS := \
  -DPLATFORM_PC -DPLATFORM_DC -DNDEBUG -O2 \
  -DP64_PROJECT_PATH=\"$(P64_PROJECT_PATH)\" \
  -I$(RT) -I$(RT)/include \
  -I$(ENGINE)/include -I$(ENGINE)/src \
  -I$(LD_INC) -I$(T3D) \
  -I$(P64_PROJECT_PATH)/src \
  -ffunction-sections -fdata-sections

CFLAGS   += $(COMMON_FLAGS)
CXXFLAGS += $(COMMON_FLAGS) -std=gnu++20 -fexceptions -fno-rtti \
  -include $(RT)/include/pc_compat.h
LDFLAGS  += -Wl,--gc-sections

.PHONY: all clean cdi elf dirs

all: dirs elf cdi

dirs:
	mkdir -p "$(OUTDIR)" "$(OBJDIR)"

elf: $(TARGET)

define MAKE_CPP_RULE
$(call pathobj,$(1)): $(1) | dirs
	kos-c++ $$(CFLAGS) $$(CXXFLAGS) -c $$< -o $$@
endef

define MAKE_C_RULE
$(call pathobj,$(1)): $(1) | dirs
	kos-cc $$(CFLAGS) -c $$< -o $$@
endef

$(foreach s,$(ENGINE_CPP) $(RT_CPP) $(GAME_CPP),$(eval $(call MAKE_CPP_RULE,$(s))))
$(foreach s,$(RT_C),$(eval $(call MAKE_C_RULE,$(s))))

$(TARGET): $(OBJS) | dirs
	kos-c++ $(CFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $(KOS_START) $(OBJS) $(KOS_LIBS)
	@echo "Built $@"

cdi: $(TARGET)
	@if command -v mkdcdisc >/dev/null 2>&1; then \
		mkdcdisc -e "$(TARGET)" -o "$(CDI)" -D "$(P64_PROJECT_PATH)/filesystem" -n "$(P64_ROM_NAME)" -N; \
		echo "Built $(CDI)"; \
	else \
		echo "mkdcdisc not found; ELF is at $(TARGET) — install mkdcdisc for CDI packaging"; \
	fi

clean:
	rm -rf "$(OBJDIR)" "$(TARGET)" "$(CDI)"
