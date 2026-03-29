CC := cc
CFLAGS := -std=c11 -O2 -Wall -Wextra -pedantic
PKG_CFLAGS := $(shell pkg-config --cflags ddsc 2>/dev/null || pkg-config --cflags CycloneDDS)
PKG_LIBS := $(shell pkg-config --libs ddsc 2>/dev/null || pkg-config --libs CycloneDDS)

SRC_DIR := src
BIN_DIR := bin

IDL := AgentState.idl
GEN_C := $(SRC_DIR)/AgentState.c
GEN_H := $(SRC_DIR)/AgentState.h

TARGETS := $(BIN_DIR)/agent $(BIN_DIR)/viewer

all: $(TARGETS)

$(GEN_C) $(GEN_H): $(SRC_DIR)/$(IDL)
	cd src && idlc -l c $(IDL)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/agent: $(SRC_DIR)/agent.c $(GEN_C) $(GEN_H) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -I$(SRC_DIR) -o $@ $(SRC_DIR)/agent.c $(GEN_C) $(PKG_LIBS) -lm

$(BIN_DIR)/viewer: $(SRC_DIR)/viewer.c $(GEN_C) $(GEN_H) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -I$(SRC_DIR) -o $@ $(SRC_DIR)/viewer.c $(GEN_C) $(PKG_LIBS)

run: all
	./run.sh

clean:
	rm -f $(TARGETS) $(GEN_C) $(GEN_H)

.PHONY: all clean run
