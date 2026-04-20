# lace-conformance — build the test harness binary.
#
# Sources: main + vector loading + adapter + diff + runner + mock HTTP/HTTPS
# server + JUnit reporter + manifest + cJSON.

# Read version from the canonical VERSION file at the repo root.
LACE_VERSION := $(shell cat ../../VERSION 2>/dev/null || echo 0.9.0)

CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -O2 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
CFLAGS  += -Ithird_party/cjson
CFLAGS  += -DLACE_CONFORMANCE_VERSION='"$(LACE_VERSION)"'
LDFLAGS ?= -lpthread
BUILD   := build

# Opt-in compile-time vector snapshot. When set, vectors/ is transcoded to
# $(BUILD)/embedded_vectors.c by tools/embed_vectors and linked in.
# Distribution packages should always ship with EMBED_VECTORS=1.
EMBED_VECTORS ?= 0
VECTORS_ROOT  ?= ../vectors

# TLS is mandatory — the mock server always supports HTTPS scenarios.
# Requires OpenSSL dev headers and libraries (apt install libssl-dev /
# brew install openssl).
CFLAGS  += -DWITH_TLS
LDFLAGS += -lssl -lcrypto

SRCS := \
	main.c           \
	vector.c         \
	adapter.c        \
	diff.c           \
	runner.c         \
	mock_server.c    \
	reporter_junit.c \
	manifest.c       \
	third_party/cjson/cJSON.c

ifeq ($(EMBED_VECTORS),1)
SRCS += $(BUILD)/embedded_vectors.c
# Generated vectors blob routinely exceeds C99's 4095-char string-literal
# minimum. All modern compilers support much longer literals in practice.
CFLAGS += -Wno-overlength-strings
endif

OBJS := $(SRCS:%.c=$(BUILD)/%.o)

TARGET := $(BUILD)/lace-conformance

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: %.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Build the embed tool, then use it to generate the vectors snapshot.
$(BUILD)/embed_vectors: tools/embed_vectors.c | $(BUILD)
	$(CC) -std=c99 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -o $@ $<

$(BUILD)/embedded_vectors.c: $(BUILD)/embed_vectors $(shell find $(VECTORS_ROOT) -name '*.json' 2>/dev/null) | $(BUILD)
	$(BUILD)/embed_vectors $(VECTORS_ROOT) $@

$(BUILD):
	mkdir -p $(BUILD)

# Smoke test — currently just runs --version.
test: $(TARGET)
	$(TARGET) --version
	$(TARGET) --help > /dev/null

clean:
	rm -rf $(BUILD)
