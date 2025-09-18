SRC          ?= .
SRC          := $(abspath $(SRC))
DESTDIR      ?= $(SRC)/lib
CC           ?= gcc
LUA          ?= lua
BEAR         ?= bear
GREP         ?= grep
CFLAGS       ?= -O3 -flto -Wl,-s -Winline

CFLAGS       += -x c -fPIC -shared -I"$(LUA_INCDIR)"
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

build: $(SRC)/src/*
	$(check_lua_incdir)
	@mkdir -p $(DESTDIR)
	$(CC) $(CFLAGS) -o $(DESTDIR)/tomlua.so $(SRCS)

bear:
	$(check_lua_incdir)
	$(BEAR) -- $(CC) -### $(CFLAGS) -o $(DESTDIR)/tomlua.so $(SRCS) > /dev/null 2>&1
	$(GREP) -v -- "-###" compile_commands.json > compile_commands.tmp && mv compile_commands.tmp compile_commands.json

test: $(SRC)/src/* $(TESTDIR)/*
	$(check_so_was_built)
	$(LUA) "$(TESTDIR)/run_tests.lua" -- "$(TESTDIR)" "$(DESTDIR)"

install:
ifdef LIBDIR
	$(check_so_was_built)
	@mkdir -p "$(LIBDIR)";
	cp "$(DESTDIR)/tomlua.so" "$(LIBDIR)/";
	@echo "Installed to $(LIBDIR)";
else
	@echo "LIBDIR not set, skipping install"
endif

clean:
	rm -rf $(DESTDIR) compile_commands.json compile_commands.tmp
