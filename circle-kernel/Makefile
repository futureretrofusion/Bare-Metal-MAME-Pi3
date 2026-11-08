CIRCLESTDLIB ?= ../circle-stdlib
include $(CIRCLESTDLIB)/Config.mk

FRF_VCHI_MODE_QUERY ?= 0
ifeq ($(FRF_VCHI_MODE_QUERY),1)
DEFINE += -DFRF_VCHI_MODE_QUERY=1
EXTRAINCLUDE += -I$(CIRCLEHOME)/addon/vc4
endif
FRF_VSYNC_BATCH_TAGS ?= 1
ifeq ($(FRF_VSYNC_BATCH_TAGS),1)
DEFINE += -DFRF_VSYNC_BATCH_TAGS=1
endif
FRF_BM_ASYNC_RENDER ?= 0
ifeq ($(FRF_BM_ASYNC_RENDER),1)
DEFINE += -DFRF_BM_ASYNC_RENDER=1
endif
FRF_BM_ASYNC_VSYNC ?= 0
ifeq ($(FRF_BM_ASYNC_VSYNC),1)
DEFINE += -DFRF_BM_ASYNC_VSYNC=1
endif
FRF_MAME_AUDIO_FAST ?= 1
ifeq ($(FRF_MAME_AUDIO_FAST),1)
DEFINE += -DFRF_MAME_AUDIO_FAST=1
endif
FRF_MAME_FAST ?= 1
ifeq ($(FRF_MAME_FAST),1)
DEFINE += -DFRF_MAME_FAST=1
endif
DEFINE += -DARM_ALLOW_MULTI_CORE
# Experimental exclusive TinyUSB DWC2 host backend (Circle owns SD, video,
# timers, HDMI; TinyUSB alone owns USB DWC2). Default remains tested Circle.
FRF_TINYUSB_MAME_HOST ?= 1
ifeq ($(FRF_TINYUSB_MAME_HOST),1)
DEFINE += -DFRF_TINYUSB_MAME_HOST=1 -DFRF_USB_TINYUSB=1
EXTRAINCLUDE += -I../third_party/frf_tinyusb_mame/src -I../baremetal/tinyusb
endif


OBJS = main.o kernel.o frf_gui.o workers.o bm_video.o bm_timer_circle.o bm_osd.o frf_frameskip.o bm_logger.o screen_dbuf.o
ifeq ($(FRF_VCHI_MODE_QUERY),1)
OBJS += frf_vc4_modes.o
endif
ifeq ($(FRF_TINYUSB_MAME_HOST),1)
OBJS += frf_tusb_host.o tusb_core.o tusb_fifo.o tusb_usbh.o tusb_hub.o \
        tusb_hid_host.o tusb_dwc2.o tusb_dwc2_common.o
endif


EXTRAINCLUDE += -I../mame106 -I../baremetal -I../third_party/zlib

include $(CIRCLEHOME)/Rules.mk

INCLUDE := $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE)

LIBS := \
    $(if $(filter 1,$(FRF_VCHI_MODE_QUERY)),$(CIRCLEHOME)/addon/vc4/interface/vmcs_host/libvmcs_host.a) \
    $(if $(filter 1,$(FRF_VCHI_MODE_QUERY)),$(CIRCLEHOME)/addon/vc4/interface/vcos/libvcos.a) \
    $(if $(filter 1,$(FRF_VCHI_MODE_QUERY)),$(CIRCLEHOME)/addon/vc4/vchiq/libvchiq.a) \
    $(if $(filter 1,$(FRF_VCHI_MODE_QUERY)),$(CIRCLEHOME)/addon/linux/liblinuxemu.a) \
    $(if $(filter 1,$(FRF_VCHI_MODE_QUERY)),$(CIRCLEHOME)/lib/sched/libsched.a) \
    ../build-bm-dkong-arm32-v01/libmamecore.a \
    ../build-bm-dkong-arm32-v01/libmamecoreUNROLL.a \
    ../build-bm-dkong-arm32-v01/zlib/libz.a \
    ../build-bm-dkong-arm32-v01/libexpat/libexpat.a \
    $(CIRCLEHOME)/lib/sound/libsound.a \
    $(CIRCLE_STDLIB_LIBS)

frf_tusb_host.o: ../baremetal/tinyusb/frf_tusb_host.cpp ../baremetal/tinyusb/frf_hid_gamepad.h ../baremetal/tinyusb/frf_known_pads.h
	$(CPP) $(CPPFLAGS) -c -o $@ $<

tusb_core.o: ../third_party/frf_tinyusb_mame/src/tusb.c
	$(CC) $(CFLAGS) $(C_STANDARD) -c -o $@ $<

tusb_fifo.o: ../third_party/frf_tinyusb_mame/src/common/tusb_fifo.c
	$(CC) $(CFLAGS) $(C_STANDARD) -c -o $@ $<

tusb_usbh.o: ../third_party/frf_tinyusb_mame/src/host/usbh.c
	$(CC) $(CFLAGS) $(C_STANDARD) -c -o $@ $<

tusb_hub.o: ../third_party/frf_tinyusb_mame/src/host/hub.c
	$(CC) $(CFLAGS) $(C_STANDARD) -c -o $@ $<

tusb_hid_host.o: ../third_party/frf_tinyusb_mame/src/class/hid/hid_host.c
	$(CC) $(CFLAGS) $(C_STANDARD) -c -o $@ $<

tusb_dwc2.o: ../third_party/frf_tinyusb_mame/src/portable/synopsys/dwc2/hcd_dwc2.c
	$(CC) $(CFLAGS) $(C_STANDARD) -c -o $@ $<

tusb_dwc2_common.o: ../third_party/frf_tinyusb_mame/src/portable/synopsys/dwc2/dwc2_common.c
	$(CC) $(CFLAGS) $(C_STANDARD) -c -o $@ $<

frf_vc4_modes.o: frf_vc4_modes.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

screen_dbuf.o: screen_dbuf.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

bm_video.o: ../baremetal/bm_video.c
	$(CC) $(CFLAGS) $(C_STANDARD) -I../baremetal -c -o $@ $<

bm_timer_circle.o: ../baremetal/bm_timer_circle.cpp
	$(CPP) $(CPPFLAGS) -I../baremetal -c -o $@ $<

bm_osd.o: ../baremetal/bm_osd.cpp
	$(CPP) $(CPPFLAGS) -I../baremetal -I../mame106 -c -o $@ $<

-include $(DEPS)

bm_logger.o: bm_logger.cpp
	$(CPP) $(CPPFLAGS) -I../baremetal -c -o $@ $<

frf_frameskip.o: ../baremetal/frf_frameskip.c
	$(CC) $(CFLAGS) $(C_STANDARD) -I../baremetal -c -o $@ $<

frf_gui.o: frf_gui.cpp frf_gui.h frf_gui_font.h frf_gui_banner.h
	$(CPP) $(CPPFLAGS) -c -o $@ $<
