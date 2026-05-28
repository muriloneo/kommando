# Kommando — pure-C host tests
#
# These tests build with the system gcc and run on the dev machine — no
# ESP-IDF toolchain, no QEMU, no emulator. The intent is fast feedback on
# the parts of the firmware that have zero ESP-IDF dependencies (pure
# logic modules and parsers).
#
# Usage:
#   make -C firmware/host_test           # build + run all tests
#   make -C firmware/host_test clean
#
# Add a new test by listing it in TESTS below and creating a matching
# test_<name>.c in this directory. The source under test goes in SRC_<name>.

CC      := gcc
CSTD    := -std=c11
WARN    := -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter
OPT     := -O0 -g
INC     := -I../main -I.
CFLAGS  := $(CSTD) $(WARN) $(OPT) $(INC)

BUILD   := build

TESTS   := commands_parse

SRC_commands_parse := ../main/commands_parse.c

all: $(addprefix $(BUILD)/test_, $(TESTS))
	@echo "==> Running host tests"
	@fail=0; for t in $^; do \
	  echo "-- $$t"; \
	  $$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then \
	  echo "HOST TESTS FAILED"; exit 1; \
	else \
	  echo "ALL HOST TESTS PASSED"; \
	fi

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/test_commands_parse: test_commands_parse.c $(SRC_commands_parse) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
