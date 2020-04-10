TARGET_LIB = libvitaSAS.a
OBJS       = source/SAS.o source/audio_out.o source/audio_dec_common.o source/audio_dec_at9.o source/audio_dec_mp3.o source/audio_dec_aac.o
INCLUDES   = include

PREFIX  ?= ${DOLCESDK}/arm-dolce-eabi
CC      = arm-dolce-eabi-gcc
AR      = arm-dolce-eabi-ar
CFLAGS  = -Wl,-q -Wall -O3 -I$(INCLUDES) -ffat-lto-objects -flto
ASFLAGS = $(CFLAGS)

all: $(TARGET_LIB)

debug: CFLAGS += -DDEBUG_BUILD
debug: all

$(TARGET_LIB): $(OBJS)
	$(AR) -rc $@ $^

clean:
	rm -rf $(TARGET_LIB) $(OBJS)

install: $(TARGET_LIB) 
	@mkdir -p $(DESTDIR)$(PREFIX)/include/psp2/
	cp include/psp2/sas.h $(DESTDIR)$(PREFIX)/include/psp2/
	cp include/psp2/codecengine.h $(DESTDIR)$(PREFIX)/include/psp2/
	@mkdir -p $(DESTDIR)$(PREFIX)/lib/
	cp $(TARGET_LIB) $(DESTDIR)$(PREFIX)/lib/
	@mkdir -p $(DESTDIR)$(PREFIX)/include/
	cp include/vitaSAS.h $(DESTDIR)$(PREFIX)/include/
