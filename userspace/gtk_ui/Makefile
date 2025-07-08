# Makefile for GTK Chat Applications
# Linux Driver Development Project

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-2.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-2.0)
PTHREAD_LIBS = -lpthread

# Include parent directory for crypto_lib
INCLUDES = -I..

# Source files
CLIENT_SRC = chat_client_gtk.c ../crypto_lib.c
SERVER_SRC = chat_server_gtk.c ../crypto_lib.c

# Target executables
CLIENT_TARGET = chat_client_gtk
SERVER_TARGET = chat_server_gtk

# Default target
all: $(CLIENT_TARGET) $(SERVER_TARGET)

# Build client
$(CLIENT_TARGET): $(CLIENT_SRC) ../crypto_lib.h
	@echo "Building GTK chat client..."
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(INCLUDES) -o $(CLIENT_TARGET) $(CLIENT_SRC) $(GTK_LIBS) $(PTHREAD_LIBS)
	@echo "✓ GTK chat client built successfully"

# Build server
$(SERVER_TARGET): $(SERVER_SRC) ../crypto_lib.h
	@echo "Building GTK chat server..."
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(INCLUDES) -o $(SERVER_TARGET) $(SERVER_SRC) $(GTK_LIBS) $(PTHREAD_LIBS)
	@echo "✓ GTK chat server built successfully"

# Clean targets
clean:
	@echo "Cleaning GTK applications..."
	rm -f $(CLIENT_TARGET) $(SERVER_TARGET)
	rm -f *.o
	@echo "✓ Clean complete"

# Install targets (copy to parent directory for easier access)
install: all
	@echo "Installing GTK applications..."
	cp $(CLIENT_TARGET) ../
	cp $(SERVER_TARGET) ../
	@echo "✓ GTK applications installed"

# Check dependencies
check-deps:
	@echo "Checking GTK dependencies..."
	@pkg-config --exists gtk+-2.0 || (echo "GTK+ 2.0 not found. Install with: sudo yum install gtk2-devel" && exit 1)
	@echo "✓ GTK+ 2.0 found"
	@gcc --version > /dev/null || (echo "GCC not found" && exit 1)
	@echo "✓ GCC found"

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build both client and server"
	@echo "  client       - Build only GTK client"
	@echo "  server       - Build only GTK server" 
	@echo "  clean        - Remove built files"
	@echo "  install      - Copy executables to parent directory"
	@echo "  check-deps   - Check for required dependencies"
	@echo "  help         - Show this help"

# Individual targets
client: $(CLIENT_TARGET)
server: $(SERVER_TARGET)

.PHONY: all clean install check-deps help client server
