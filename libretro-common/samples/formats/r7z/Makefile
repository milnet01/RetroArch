TARGETS := r7z_lzma_props_test r7z_folder_info_test r7z_dict_reset_test

LIBRETRO_COMM_DIR := ../../..

R7Z_SOURCES := \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_archive.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_bcj2.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_filters.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma2.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma_stream.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c

PROPS_OBJS := r7z_lzma_props_test.o \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma.o

FOLDER_INFO_OBJS := r7z_folder_info_test.o $(R7Z_SOURCES:.c=.o)

DICT_RESET_OBJS := r7z_dict_reset_test.o $(R7Z_SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

r7z_lzma_props_test: $(PROPS_OBJS)
	$(CC) $(PROPS_OBJS) $(LDFLAGS) -o $@

r7z_folder_info_test: $(FOLDER_INFO_OBJS)
	$(CC) $(FOLDER_INFO_OBJS) $(LDFLAGS) -o $@

r7z_dict_reset_test: $(DICT_RESET_OBJS)
	$(CC) $(DICT_RESET_OBJS) $(LDFLAGS) -o $@

check: $(TARGETS)
	./r7z_lzma_props_test
	./r7z_folder_info_test
	./r7z_dict_reset_test

clean:
	rm -f $(TARGETS) $(PROPS_OBJS) $(FOLDER_INFO_OBJS) r7z_dict_reset_test.o

.PHONY: all check clean
