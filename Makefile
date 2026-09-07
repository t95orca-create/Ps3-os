TARGET      :=  ps5_dashboard
BUILD       :=  build
SOURCES     :=  source
DATA        :=  assets
INCLUDES    :=  source

CFLAGS      =   -O2 -Wall $(MACHDEP)
CXXFLAGS    =   $(CFLAGS)

LIBS        := -lrsx -lgcm_sys -lsysutil -lio -lm

include $(PS3DEV)/ppu_rules

all: $(TARGET).self

%.self: %.elf
	$(STRIP) -o $(TARGET).stripped.elf $<
	sprxlinker $(TARGET).stripped.elf
	make_self_npdrm $(TARGET).stripped.elf $@ 0000000000000000
