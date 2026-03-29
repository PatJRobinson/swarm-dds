CC := cc
CFLAGS := -std=c11 -O2 -Wall -Wextra -pedantic
PKG_CFLAGS := $(shell pkg-config --cflags ddsc 2>/dev/null || pkg-config --cflags CycloneDDS)
PKG_LIBS := $(shell pkg-config --libs ddsc 2>/dev/null || pkg-config --libs CycloneDDS)

ROOT_DIR := $(abspath .)
SRC_DIR := src
GEN_DIR := generated
BIN_DIR := bin

IDL := AgentState.idl
IDL_SRC := $(ROOT_DIR)/$(SRC_DIR)/$(IDL)
GEN_C := $(GEN_DIR)/AgentState.c
GEN_H := $(GEN_DIR)/AgentState.h

TARGETS := $(BIN_DIR)/agent $(BIN_DIR)/viewer

all: $(TARGETS)

$(GEN_DIR):
	mkdir -p $(GEN_DIR)

$(GEN_C) $(GEN_H): $(SRC_DIR)/$(IDL) | $(GEN_DIR)
	cd $(GEN_DIR) && idlc -l c $(IDL_SRC)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/agent: $(SRC_DIR)/agent.c $(GEN_C) $(GEN_H) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -I$(SRC_DIR) -o $@ $(SRC_DIR)/agent.c $(GEN_C) $(PKG_LIBS) -lm

$(BIN_DIR)/viewer: $(SRC_DIR)/viewer.c $(GEN_C) $(GEN_H) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -I$(SRC_DIR) -o $@ $(SRC_DIR)/viewer.c $(GEN_C) $(PKG_LIBS)

run: all
	./run.sh

clean:
	rm -rf $(TARGETS) $(GEN_C) $(GEN_H) $(GEN_DIR) $(BIN_DIR)

.PHONY: all clean run
