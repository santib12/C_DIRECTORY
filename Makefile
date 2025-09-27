# Makefile for NEXUS TERMINAL - Linux Edition
# Simple Console Terminal

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2

# Source files
SIMPLE_SRC = src/simple_console_terminal.c
SIMPLE_TARGET = build/simple_terminal

# Default target
all: $(SIMPLE_TARGET)

# Build the simple console terminal
$(SIMPLE_TARGET): $(SIMPLE_SRC)
	@echo "Building NEXUS TERMINAL Simple Console for Linux..."
	@mkdir -p build
	$(CC) $(CFLAGS) -o $(SIMPLE_TARGET) $(SIMPLE_SRC)
	@echo "Simple console terminal built: $(SIMPLE_TARGET)"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf build/
	@echo "Clean complete!"

# Install dependencies (Ubuntu/Debian)
install-deps:
	@echo "Installing required dependencies..."
	sudo apt-get update
	sudo apt-get install -y build-essential
	@echo "Dependencies installed!"

# Run the simple console terminal
run-simple: $(SIMPLE_TARGET)
	@echo "Starting NEXUS TERMINAL Simple Console..."
	./$(SIMPLE_TARGET)

# Launch simple terminal in native window
launch-simple: $(SIMPLE_TARGET)
	@echo "Launching NEXUS TERMINAL Simple Console in native window..."
	gnome-terminal --title="NEXUS Terminal" -- ./$(SIMPLE_TARGET) 2>/dev/null || \
	xfce4-terminal --title="NEXUS Terminal" --command="./$(SIMPLE_TARGET)" 2>/dev/null || \
	konsole --title="NEXUS Terminal" -e ./$(SIMPLE_TARGET) 2>/dev/null || \
	xterm -title "NEXUS Terminal" -e ./$(SIMPLE_TARGET) 2>/dev/null || \
	mate-terminal --title="NEXUS Terminal" --command="./$(SIMPLE_TARGET)" 2>/dev/null || \
	./$(SIMPLE_TARGET)

# Help
help:
	@echo "Available targets:"
	@echo "  all          - Build simple console terminal (default)"
	@echo "  simple       - Build simple console terminal only"
	@echo "  clean        - Remove build artifacts"
	@echo "  install-deps - Install required dependencies"
	@echo "  run-simple   - Build and run simple console terminal"
	@echo "  launch-simple - Build and launch simple terminal in native window"
	@echo "  help         - Show this help message"

# Individual target
simple: $(SIMPLE_TARGET)

.PHONY: all simple clean install-deps run-simple launch-simple help