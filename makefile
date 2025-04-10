# Compiler and flags
CC = gcc
CFLAGS = -pthread -Wall -Wextra -g

# Target and source files
TARGET = chash
SRCS = chash.c
HEADERS = hashing.h

# Default target
all: $(TARGET)

# Build the target
$(TARGET): $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Clean up
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all clean run
