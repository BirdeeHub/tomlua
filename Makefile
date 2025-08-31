ifndef LUA_INCDIR
$(error LUA_INCDIR header files not provided. Please pass or export LUA_INCDIR=/path/to/lua/include)
endif
CC         ?= gcc
CFLAGS     = -O3 -fPIC -flto -finline-functions -shared -Wl,-s
DESTDIR    ?= ./lib
SRCS       = ./src/tomlua.c ./src/parse_str.c ./src/parse_keys.c ./src/parse_val.c ./src/decode.c ./src/encode.c
TESTDIR    ?= ./tests
TEST       = "$(TESTDIR)/init.lua"
INCLUDES   = -I"$(LUA_INCDIR)"
LUA        ?= lua
ifndef LUA_BINDIR
LUA_BIN ?= "$(LUA)"
else
LUA_BIN ?= "$(LUA_BINDIR)/$(LUA)"
endif

all: build test

test: $(SRCS) $(TESTS)
	$(LUA_BIN) "$(TEST)" -- "$(TESTDIR)" "$(DESTDIR)"

build: $(SRCS)
	@mkdir -p $(DESTDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(DESTDIR)/tomlua.so $(SRCS)

bear: clean
	@mkdir -p $(DESTDIR)
	bear -- $(CC) $(CFLAGS) $(INCLUDES) -o $(DESTDIR)/tomlua.so $(SRCS)

clean:
	rm -rf $(DESTDIR) compile_commands.json
