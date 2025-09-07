SRC          ?= .
DESTDIR      ?= $(SRC)/lib
TEMPDIR      ?= $(SRC)/tmp
CC           ?= gcc
LUA          ?= lua
BEAR         ?= bear
GREP         ?= grep
CFLAGS       ?= -O3 -flto -finline-functions -Wl,-s

EMBEDDED_LUA = $(TEMPDIR)/embedded.h
CFLAGS       += -fPIC -shared -DEMBEDDED_LUA="$(EMBEDDED_LUA)" -I"$(LUA_INCDIR)"
TESTDIR      = $(SRC)/tests
SRCS         = $(SRC)/src/tomlua.c \
               $(SRC)/src/parse_str.c \
               $(SRC)/src/decode.c

all: build test

test: $(SRC)/src/* $(SRC)/pkg/* $(TESTDIR)/*
	@if [ ! -f "$(DESTDIR)/tomlua.so" ]; then \
		echo "Error: $(DESTDIR)/tomlua.so not built. Run make build first."; \
		false; \
	fi
	$(LUA) "$(TESTDIR)/init.lua" -- "$(TESTDIR)" "$(DESTDIR)"

install:
ifdef LIBDIR
	@if [ ! -f "$(DESTDIR)/tomlua.so" ]; then \
		echo "Error: $(DESTDIR)/tomlua.so not built. Run make build first."; \
		false; \
	fi
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

embed: $(SRC)/src/encode.lua $(SRC)/pkg/*
	@if [ -z "$(LUA_INCDIR)" ]; then \
		echo "Error: LUA_INCDIR not set. Please pass or export LUA_INCDIR=/path/to/lua/include"; \
		false; \
	fi
	@mkdir -p $(TEMPDIR)
	$(CC) $(CFLAGS) -o "$(TEMPDIR)/embed_lua.so" "$(SRC)/pkg/embed_lua.c"
	$(LUA) $(SRC)/pkg/embed.lua "$(TEMPDIR)/embed_lua.so" "$(SRC)/src" "$(EMBEDDED_LUA)"

build: $(SRC)/src/* embed
	@if [ -z "$(LUA_INCDIR)" ]; then \
		echo "Error: LUA_INCDIR not set. Please pass or export LUA_INCDIR=/path/to/lua/include"; \
		false; \
	fi
	@mkdir -p $(DESTDIR)
	$(CC) $(CFLAGS) -o $(DESTDIR)/tomlua.so $(SRCS)

bear:
	$(BEAR) -- $(CC) -### $(CFLAGS) -o $(DESTDIR)/tomlua.so $(SRCS) > /dev/null 2>&1
	$(GREP) -v -- "-###" compile_commands.json > compile_commands.tmp && mv compile_commands.tmp compile_commands.json

clean:
	rm -rf $(DESTDIR) $(TEMPDIR) compile_commands.json
