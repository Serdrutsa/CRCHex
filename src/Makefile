# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -g -I/usr/include/
TARGET = CRCHex

# List of source files
SRCS = CRCHex.cpp

# List of object files (derived from SRCS)
OBJS = $(SRCS:.cpp=.o)

# Phony targets (not actual files)
.PHONY: all clean

# Default target: build the program
all: $(TARGET)

# Rule to build the final executable from object files
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Rule to build object files from source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)
