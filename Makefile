CC      := gcc
WINCC   := x86_64-w64-mingw32-gcc
CFLAGS  := -Wall -Wextra -O2 -I./headers/common
BUILD   := build
WLIBS   := -lws2_32

.PHONY: all clean client relay client_win relay_win

all: client relay client_win relay_win

client: clients/client0/client.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $< -o $(BUILD)/client

relay: relays/relay0/relay.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $< -o $(BUILD)/relay

client_win: clients/client0/client.c
	@mkdir -p $(BUILD)
	$(WINCC) $(CFLAGS) $< -o $(BUILD)/client.exe $(WLIBS)

relay_win: relays/relay0/relay.c
	@mkdir -p $(BUILD)
	$(WINCC) $(CFLAGS) $< -o $(BUILD)/relay.exe $(WLIBS)

clean:
	rm -rf $(BUILD)