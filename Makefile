#!

CROSS_PREFIX ?= x86_64-w64-mingw32-
CC ?= gcc
WINECC ?= winegcc

ifeq ($(DEBUG),1)
_DEBUG = true
else
_DEBUG = $(DEBUG)
endif

ifeq ($(_DEBUG),true)
CFLAGS += -O0 -g -DDEBUG
else
CFLAGS += -O2 -DNDEBUG
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -fvisibility=hidden
LDFLAGS += -Wl,-O1,--as-needed,--gc-sections,--no-undefined,--strip-all
endif

CFLAGS += -mms-bitfields
CFLAGS += -mstackrealign
CFLAGS += -std=gnu11
CFLAGS += -Wall -Wextra
# CFLAGS += -Wno-cast-function-type
CFLAGS += -Wno-incompatible-pointer-types
CFLAGS += -Wno-unused-parameter

MINGW_CFLAGS += $(CFLAGS)
MINGW_CFLAGS += -Wpedantic-ms-format

WINE_CFLAGS += $(CFLAGS)
WINE_CFLAGS += -mno-cygwin

WINE_LDFLAGS += $(LDFLAGS)
WINE_LDFLAGS += -ldl

X11_CFLAGS = $(shell pkg-config --cflags x11)
X11_LDFLAGS = $(shell pkg-config --libs x11)

all: bin/winesulin.dll bin/winesulin.dll.so

bin/winesulin.dll: winesulin-init.c winesulin.def
	@install -d bin
	$(CROSS_PREFIX)gcc $^ $(MINGW_CFLAGS) $(LDFLAGS) -static -shared -o $@

bin/winesulin.dll.so: winesulin-wrapper.c winesulin.def
	@install -d bin
	$(WINECC) $^ $(WINE_CFLAGS) $(X11_CFLAGS) $(WINE_LDFLAGS) $(X11_LDFLAGS) -shared -o $@

clean:
	rm -f bin/winesulin.dll bin/winesulin.dll.so
