#
# Students' Makefile for the Malloc Lab
#

TEAM = bovik
VERSION = 1
HANDINDIR = /afs/cs.cmu.edu/academic/class/15213-f01/malloclab/handin

CC = gcc
# CFLAGS = -Wall -O2 -m32
CFLAGS = -Wall -O0 -g

# Output directories
OBJ_DIR = out/obj
BIN_DIR = out/bin

# Object and binary targets
OBJS = $(OBJ_DIR)/mdriver.o $(OBJ_DIR)/mm.o $(OBJ_DIR)/memlib.o \
       $(OBJ_DIR)/fsecs.o $(OBJ_DIR)/fcyc.o $(OBJ_DIR)/clock.o $(OBJ_DIR)/ftimer.o

TARGET = $(BIN_DIR)/mdriver

# Default target
all: $(TARGET)

# Binary build rule
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Compile rules for each object file
$(OBJ_DIR)/mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ mdriver.c

$(OBJ_DIR)/memlib.o: memlib.c memlib.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ memlib.c

$(OBJ_DIR)/mm.o: mm.c mm.h memlib.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ mm.c

$(OBJ_DIR)/fsecs.o: fsecs.c fsecs.h config.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ fsecs.c

$(OBJ_DIR)/fcyc.o: fcyc.c fcyc.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ fcyc.c

$(OBJ_DIR)/ftimer.o: ftimer.c ftimer.h config.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ ftimer.c

$(OBJ_DIR)/clock.o: clock.c clock.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ clock.c

# Ensure directories exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Submission rule
handin:
	cp mm.c $(HANDINDIR)/$(TEAM)-$(VERSION)-mm.c

# Clean rule
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	rm -f *~ 
