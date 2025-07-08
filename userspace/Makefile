CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -m64 -D_GNU_SOURCE
LDFLAGS = -pthread -m64

# Source files
CRYPTO_LIB_SRC = crypto_lib.c
SERVER_SRC = chat_server.c
CLIENT_SRC = chat_client.c
TEST_SRC = test_crypto.c

# Object files
CRYPTO_LIB_OBJ = crypto_lib.o
SERVER_OBJ = chat_server.o
CLIENT_OBJ = chat_client.o
TEST_OBJ = test_crypto.o

# Executables
SERVER_EXEC = chat_server
CLIENT_EXEC = chat_client
TEST_EXEC = test_crypto

# Default target - build both CLI and GTK versions
all: cli gtk

# Build CLI versions only
cli: $(SERVER_EXEC) $(CLIENT_EXEC) $(TEST_EXEC)

# Build GTK versions
gtk:
	@echo "Building GTK applications..."
	@if [ -d gtk_ui ]; then \
		cd gtk_ui && $(MAKE) all && $(MAKE) install; \
	else \
		echo "GTK UI directory not found. Creating basic structure..."; \
		mkdir -p gtk_ui; \
		echo "Please add GTK source files to gtk_ui/ directory"; \
	fi

# Build server
$(SERVER_EXEC): $(SERVER_OBJ) $(CRYPTO_LIB_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Build client
$(CLIENT_EXEC): $(CLIENT_OBJ) $(CRYPTO_LIB_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Build test program
$(TEST_EXEC): $(TEST_OBJ) $(CRYPTO_LIB_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Object file rules
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean build files
clean:
	rm -f *.o $(SERVER_EXEC) $(CLIENT_EXEC) $(TEST_EXEC)
	@if [ -d gtk_ui ]; then cd gtk_ui && $(MAKE) clean; fi

# Clean all including GTK
clean-all: clean
	@if [ -d gtk_ui ]; then cd gtk_ui && $(MAKE) clean; fi
	rm -f chat_*_gtk

# Install (copy to /usr/local/bin)
install: all
	sudo cp $(SERVER_EXEC) /usr/local/bin/
	sudo cp $(CLIENT_EXEC) /usr/local/bin/
	@if [ -f chat_server_gtk ]; then sudo cp chat_server_gtk /usr/local/bin/; fi
	@if [ -f chat_client_gtk ]; then sudo cp chat_client_gtk /usr/local/bin/; fi

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/$(SERVER_EXEC)
	sudo rm -f /usr/local/bin/$(CLIENT_EXEC)
	sudo rm -f /usr/local/bin/chat_server_gtk
	sudo rm -f /usr/local/bin/chat_client_gtk

# Test crypto library
test: $(TEST_EXEC)
	./$(TEST_EXEC)

# Check GTK dependencies
check-gtk:
	@if [ -d gtk_ui ]; then cd gtk_ui && $(MAKE) check-deps; fi

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build both CLI and GTK versions"
	@echo "  cli          - Build CLI versions only"
	@echo "  gtk          - Build GTK versions only"
	@echo "  clean        - Clean CLI build files"
	@echo "  clean-all    - Clean all build files including GTK"
	@echo "  install      - Install all applications"
	@echo "  uninstall    - Remove installed applications"
	@echo "  test         - Run crypto library tests"
	@echo "  check-gtk    - Check GTK dependencies"
	@echo "  help         - Show this help"

.PHONY: all cli gtk clean clean-all install uninstall test check-gtk help

.PHONY: all clean install uninstall test cli gtk
