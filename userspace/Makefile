CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -m64 -D_GNU_SOURCE
LDFLAGS = -pthread -m64

# Source files
CRYPTO_LIB_SRC = crypto_lib.c
SERVER_SRC = chat_server.c
CLIENT_SRC = chat_client.c

# Object files
CRYPTO_LIB_OBJ = crypto_lib.o
SERVER_OBJ = chat_server.o
CLIENT_OBJ = chat_client.o

# Executables
SERVER_EXEC = chat_server
CLIENT_EXEC = chat_client

# Default target
all: $(SERVER_EXEC) $(CLIENT_EXEC)

# Build server
$(SERVER_EXEC): $(SERVER_OBJ) $(CRYPTO_LIB_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Build client
$(CLIENT_EXEC): $(CLIENT_OBJ) $(CRYPTO_LIB_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Object file rules
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean build files
clean:
	rm -f *.o $(SERVER_EXEC) $(CLIENT_EXEC)

# Install (copy to /usr/local/bin)
install: all
	sudo cp $(SERVER_EXEC) /usr/local/bin/
	sudo cp $(CLIENT_EXEC) /usr/local/bin/

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/$(SERVER_EXEC)
	sudo rm -f /usr/local/bin/$(CLIENT_EXEC)

# Test crypto library
test: test_crypto
	./test_crypto

test_crypto: test_crypto.o $(CRYPTO_LIB_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

test_crypto.o: test_crypto.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: all clean install uninstall test
