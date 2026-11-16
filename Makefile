
#---------------------------------------------------------------------
# Makefile para Mini Contra - PSL1GHT
#---------------------------------------------------------------------

.SUFFIXES:

ifeq ($(strip $(PSL1GHT)),)
$(error "Define PSL1GHT en tu entorno (export PSL1GHT=$$PS3DEV)")
endif

include $(PSL1GHT)/ppu_rules

TARGET    := $(notdir $(CURDIR))
BUILD     := build
SOURCES   := source
DATA      := data
INCLUDES  := include

TITLE     := Mini Contra
APPID     := MINICONTRA
CONTENTID := UP0001-$(APPID)_00-0000000000000000
SFOXML    := sfo.xml

CFLAGS   = -O2 -Wall -mcpu=cell $(MACHDEP) $(INCLUDE)
LDFLAGS  = $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS     := -lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export BUILDDIR := $(CURDIR)/$(BUILD)
export DEPSDIR  := $(BUILDDIR)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export OFILES := $(CFILES:.c=.o)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  -I$(CURDIR)/$(SOURCES) \
                  -I$(PORTLIBS)/include \
                  -I$(PSL1GHT)/ppu/include \
                  -I$(BUILDDIR)

export LIBPATHS := -L$(PORTLIBS)/lib -L$(PSL1GHT)/ppu/lib

.PHONY: $(BUILD) clean pkg

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo limpiando...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).pkg

pkg: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $(OUTPUT).pkg SFOXML=$(CURDIR)/$(SFOXML)

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
