# Makefile for timer component

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -g -O2
LDFLAGS = 
LIBS = 

# Source files
SRCS = timer.c test.c
OBJS = $(SRCS:.c=.o)

# Targets
TARGET = test_timer

# Default target
all: $(TARGET)

# Main target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# Dependencies
timer.o: timer.c timer.h
test.o: test.c timer.h

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean