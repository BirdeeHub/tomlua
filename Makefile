SRC        ?= .
CC         ?= gcc
CFLAGS     = -O3 -fPIC -flto -finline-functions -shared -Wl,-s
DESTDIR    ?= ./lib
SRCS       = $(SRC)/src/tomlua.c \
             $(SRC)/src/parse_str.c \
             $(SRC)/src/decode.c

TESTDIR    ?= $(SRC)/tests
TEST       = $(TESTDIR)/init.lua
INCLUDES   = -I"$(LUA_INCDIR)"
LUA        ?= lua
GREP_BIN   ?= grep
BEAR_BIN   ?= bear

EMBEDDER   = $(SRC)/src/embed_lua.c
EMBEDDED   = $(SRC)/src/encode.lua
EMBED_CMD  = local embed = package.loadlib([[$(SRC)/embed/embed_lua.so]], [[luaopen_embed_lua]])(); \
             embed([[$(EMBEDDED)]], [[$(SRC)/embed/encode.h]], [[EMBED_ENCODE]], [[encode]]);

all: build test

test: $(SRCS) $(TESTS)
	@if [ ! -f "$(DESTDIR)/tomlua.so" ]; then \
		echo "Error: $(DESTDIR)/tomlua.so not built. Run make build first."; \
		false; \
	fi
	$(LUA) "$(TEST)" -- "$(TESTDIR)" "$(DESTDIR)"

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

embed: $(EMBEDDER) $(SRC)/src/encode.lua
	@mkdir -p $(SRC)/embed
	$(CC) $(CFLAGS) $(INCLUDES) -o $(SRC)/embed/embed_lua.so $(EMBEDDER)
	$(LUA) -e "$(EMBED_CMD)"

build: $(SRCS) embed
	@if [ -z "$(LUA_INCDIR)" ]; then \
		echo "Error: LUA_INCDIR not set. Please pass or export LUA_INCDIR=/path/to/lua/include"; \
		false; \
	fi
	@mkdir -p $(DESTDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(DESTDIR)/tomlua.so $(SRCS)

bear: $(SRCS) clean
	$(BEAR_BIN) -- $(CC) -### $(CFLAGS) $(INCLUDES) -o $(DESTDIR)/tomlua.so $(SRCS) > /dev/null 2>&1
	$(GREP_BIN) -v -- "-###" compile_commands.json > compile_commands.tmp && mv compile_commands.tmp compile_commands.json

clean:
	rm -rf $(DESTDIR) compile_commands.json $(SRC)/embed
