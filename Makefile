#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM)
endif

include $(DEVKITARM)/ds_rules

export TARGET		:=	$(shell basename $(CURDIR))
export TOPDIR		:=	$(CURDIR)


#---------------------------------------------------------------------------------
# path to tools - this can be deleted if you set the path in windows
#---------------------------------------------------------------------------------
export PATH		:=	$(DEVKITARM)/bin:$(PATH)

.PHONY: $(TARGET).arm7 $(TARGET).arm9

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all: $(TARGET).ds.gba

$(TARGET).ds.gba	: $(TARGET).nds

#---------------------------------------------------------------------------------
$(TARGET).nds	:	$(TARGET).arm7 $(TARGET).arm9
	$(OBJCOPY) --remove-section=.twl --remove-section=.twl_bss \
		arm7/$(TARGET).arm7.elf $(TARGET).arm7.pack.elf
	$(OBJCOPY) --remove-section=.twl --remove-section=.twl_bss \
		arm9/$(TARGET).arm9.elf $(TARGET).arm9.pack.elf
	ndstool	-c $(TARGET).nds -7 $(TARGET).arm7.pack.elf -9 $(TARGET).arm9.pack.elf

#---------------------------------------------------------------------------------
$(TARGET).arm7:
	$(MAKE) -C arm7

$(TARGET).arm9:
	$(MAKE) -C arm9

#---------------------------------------