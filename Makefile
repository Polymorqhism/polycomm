CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -O2 -IcJSON -fsanitize=address
LDLIBS = -lcurl -lpthread -lncurses -lsodium

BUILD_DIR = build

TARGET = $(BUILD_DIR)/polycomm

SRCS = polycomm.c cJSON/cJSON.c util.c server.c network.c client.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
