# Policy lanes for the self-updater (self_update.c): install kind,
# version compare, release offer, TLS gate and URLs, the Windows
# program-file filter, replace-and-rollback, the pending marker, the
# AppImage swap and start-up cleanup.  No network; the file lanes run
# in a scratch folder under this directory.
#
#   make check
#   make check SANITIZER=address,undefined
#   ./self_update_test <lane>

TARGET  := self_update_test
CORE_DIR := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES := \
	self_update_test.c \
	$(CORE_DIR)/self_update.c \
	$(LIBRETRO_COMM_DIR)/formats/json/rjson.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -Wsign-compare -std=gnu89 -g -O1 -fno-omit-frame-pointer \
           -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) -DHAVE_SELF_UPDATER

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)
	rm -rf su_scratch_*

.PHONY: all check clean
