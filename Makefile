CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -g -O0 -I.

LINK_SRC    = link_layer/link_layer.c
NET_SRC     = network_layer/network.c
TRANS_SRC   = transport_layer/transport.c
APP_SRC     = app_layer/app_layer.c
MAIN_SRC    = main.c

ALL_SRCS = $(LINK_SRC) $(NET_SRC) $(TRANS_SRC) $(APP_SRC) $(MAIN_SRC)

.PHONY: all clean

all: client

client: $(ALL_SRCS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f client
