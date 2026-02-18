CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -O2 -IcJSON -lcurl

BUILD_DIR = build

TARGET = $(BUILD_DIR)/polycomm

SRCS = polycomm.c polycomm.h cJSON/cJSON.c util.c util.h

OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

run:
	build/polycomm

.PHONY: all clean run
