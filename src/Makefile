# Compiler and flags
CC = gcc
CFLAGS = -Wall -g -I/usr/include/
TARGET = CRCHex

# List of source files
SRCS = CRCHex.c

# List of object files (derived from SRCS)
OBJS = $(SRCS:.c=.o)

# Phony targets (not actual files)
.PHONY: all clean

# Default target: build the program
all: $(TARGET)

# Rule to build the final executable from object files
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Rule to build object files from source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)
