SRC        ?= .
CC         ?= gcc
CFLAGS     = -O3 -fPIC -flto -finline-functions -shared -Wl,-s
DESTDIR    ?= ./lib
SRCS       = $(SRC)/src/tomlua.c \
             $(SRC)/src/parse_str.c \
             $(SRC)/src/parse_val.c \
             $(SRC)/src/decode.c \
             $(SRC)/src/encode.c

TESTDIR    ?= $(SRC)/tests
TEST       = $(TESTDIR)/init.lua
INCLUDES   = -I"$(LUA_INCDIR)"
LUA        ?= lua
ifndef LUA_BINDIR
LUA_BIN ?= $(LUA)
else
LUA_BIN ?= $(LUA_BINDIR)/$(LUA)
endif

all: build test

test: $(SRCS) $(TESTS)
	$(LUA_BIN) "$(TEST)" -- "$(TESTDIR)" "$(DESTDIR)"

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

build: $(SRCS)
	@if [ -z "$(LUA_INCDIR)" ]; then \
		echo "Error: LUA_INCDIR not set. Please pass or export LUA_INCDIR=/path/to/lua/include"; \
		false; \
	fi
	@mkdir -p $(DESTDIR)
	$(CC) $(CFLAGS) -I"$(LUA_INCDIR)" -o $(DESTDIR)/tomlua.so $(SRCS)

bear: $(SRCS) clean
	@mkdir -p $(DESTDIR)
	bear -- $(CC) $(CFLAGS) $(INCLUDES) -o $(DESTDIR)/tomlua.so $(SRCS)

clean:
	rm -rf $(DESTDIR) compile_commands.json
