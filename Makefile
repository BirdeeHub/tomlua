ifndef LUA_INCDIR
$(error LUA_INCDIR header files not provided. Please pass or export LUA_INCDIR=/path/to/lua/include)
endif
SRC        ?= .
CC         ?= gcc
CFLAGS     = -O3 -fPIC -flto -finline-functions -shared -Wl,-s
DESTDIR    ?= ./lib
SRCS       = $(SRC)/src/tomlua.c \
             $(SRC)/src/parse_str.c \
             $(SRC)/src/parse_keys.c \
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

build: $(SRCS)
	@mkdir -p $(DESTDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(DESTDIR)/tomlua.so $(SRCS)

bear: $(SRCS) clean
	@mkdir -p $(DESTDIR)
	bear -- $(CC) $(CFLAGS) $(INCLUDES) -o $(DESTDIR)/tomlua.so $(SRCS)

clean:
	rm -rf $(DESTDIR) compile_commands.json
