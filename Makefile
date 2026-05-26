CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -g -O0 -I.

# Fontes de cada camada
LINK_SRC    = link_layer/link.c
NET_SRC     = network_layer/network.c
TRANS_SRC   = transport_layer/transport.c
APP_SRC     = app_layer/client.c

ALL_SRCS    = $(LINK_SRC) $(NET_SRC) $(TRANS_SRC)

.PHONY: all clean

all: client

client: $(ALL_SRCS) $(APP_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f client
