# PS3 build. Mirrors D:/PS3/samples/common/gcmutil/samples/basic2/Makefile's
# structure -- see that file for what sdk.target.mk actually does.
#
# Build with MSYS2 (needed for `make`; see project plan for why):
#   "/c/msys64/usr/bin/bash.exe" -lc '
#     export PATH="/d/PS3/host-win32/spu/bin:/d/PS3/host-win32/ppu/bin:/d/PS3/host-win32/sn/bin:/d/PS3/host-win32/bin:/d/PS3/host-win32/Cg/bin:/c/Program Files (x86)/SN Systems/PS3/bin:$PATH"
#     export CELL_SDK=/d/PS3
#     export SCE_PS3_ROOT=/d/PS3
#     cd "/d/ClaudeCode/Crux PS3"
#     make'

CELL_SDK ?= /usr/local/cell
CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

SAMPLE_NAME = crux

GCM_UTIL = $(CELL_SDK)/samples/common/gcmutil
SUBDIRS = $(GCM_UTIL)

PPU_SRCS = src/main_ps3.cpp src/render_ps3.cpp src/input_ps3.cpp src/app.cpp src/level.cpp src/player.cpp src/camera2d.cpp src/score.cpp
PPU_TARGET = $(SAMPLE_NAME).ppu.elf

PPU_INCDIRS += -I$(GCM_UTIL) -I$(CELL_SDK)/samples/common/padutil -I$(CELL_SDK)/samples/common/gtf -Isrc
PPU_LIBS += $(GCM_UTIL)/gcmutil.a
PPU_LDLIBS += -lgcm_cmd -lgcm_sys_stub -lsysutil_stub -lsysmodule_stub -lfs_stub -lio_stub -lpadfilter -ldbgfont_gcm -lcgb

GCC_PPU_CXXFLAGS += -DCRUX_PS3 -fno-exceptions -fno-rtti --param large-function-growth=800

VPSHADER_SRCS = $(wildcard vs_*.cg)
FPSHADER_SRCS = $(wildcard fs_*.cg)

include $(CELL_MK_DIR)/sdk.target.mk
