SRC          ?= .
SRC          := $(abspath $(SRC))
DESTDIR      ?= $(SRC)/lib
TEMP_DIR     ?= $(SRC)/tmp
CC           ?= gcc
LUA          ?= lua
BEAR         ?= bear
GREP         ?= grep
CFLAGS       ?= -x c -O3 -flto -finline-functions -Wl,-s

EMBEDDER_SRC := $(SRC)/src/embed_lua.c
EMBEDDER     := $(TEMP_DIR)/embed_lua.so
EMBED_SCRIPT := $(SRC)/src/embed.lua
EMBEDDED_LUA := $(TEMP_DIR)/embedded.h
CFLAGS       += -fPIC -shared -I"$(LUA_INCDIR)"
TESTDIR      := $(SRC)/tests
SRCS         := $(SRC)/src/tomlua.c \
                $(SRC)/src/decode_str.c \
                $(SRC)/src/decode.c \
                $(SRC)/src/encode.c \
                $(SRC)/src/dates.c

check_lua_incdir = \
	@if [ -z "$(LUA_INCDIR)" ]; then \
		echo "Error: LUA_INCDIR not set. Please pass or export LUA_INCDIR=/path/to/lua/include"; \
		false; \
	fi

check_so_was_built = \
	@if [ ! -f "$(DESTDIR)/tomlua.so" ]; then \
		echo "Error: $(DESTDIR)/tomlua.so not built. Run make build first."; \
		false; \
	fi

all: build test

embedder: $(EMBEDDER_SRC)
	$(check_lua_incdir)
	@mkdir -p $(TEMP_DIR)
	$(CC) $(CFLAGS) -o "$(EMBEDDER)" "$(EMBEDDER_SRC)"

embed: embedder $(SRC)/src/encode.lua $(EMBED_SCRIPT)
	@mkdir -p $(TEMP_DIR)
	$(LUA) "$(EMBED_SCRIPT)" "$(EMBEDDER)" "$(SRC)/src" "$(EMBEDDED_LUA)"

build: $(SRC)/src/* embed
	$(check_lua_incdir)
	@mkdir -p $(DESTDIR)
	$(CC) $(CFLAGS) -DEMBEDDED_LUA="$(EMBEDDED_LUA)" -o $(DESTDIR)/tomlua.so $(SRCS)

bear:
	$(check_lua_incdir)
	$(BEAR) -- $(CC) -### $(CFLAGS) -DEMBEDDED_LUA="$(EMBEDDED_LUA)" -o $(DESTDIR)/tomlua.so $(SRCS) > /dev/null 2>&1
	$(GREP) -v -- "-###" compile_commands.json > compile_commands.tmp && mv compile_commands.tmp compile_commands.json

test: $(SRC)/src/* $(TESTDIR)/*
	$(check_so_was_built)
	$(LUA) "$(TESTDIR)/init.lua" -- "$(TESTDIR)" "$(DESTDIR)"

install:
ifdef LIBDIR
	$(check_so_was_built)
	@if [ -d "$(LIBDIR)" ]; then \
		cp "$(DESTDIR)/tomlua.so" "$(LIBDIR)/"; \
		echo "Installed to $(LIBDIR)"; \
	else \
		echo "LIBDIR set but does not exist: $(LIBDIR)"; \
		false; \
	fi
else
	@echo "LIBDIR not set, skipping install"
endif

clean:
	rm -rf $(DESTDIR) $(TEMP_DIR) compile_commands.json
