src          ?= .
SRC          ?= $(src)
SRC          := $(abspath $(SRC))
DESTDIR      ?= ./build/lib
CC           ?= gcc
LUA          ?= lua
BEAR         ?= bear
CFLAGS       ?= -fPIC -x c -O3 -flto -Wl,-s -Winline
LIBFLAG      ?= -shared

ifdef LUA_INC
LUA_INCDIR   ?= $(LUA_INC)
endif

CFLAGS       += $(LIBFLAG) -I"$(LUA_INCDIR)"
TESTDIR      := $(SRC)/tests
SRCS         := $(SRC)/src/tomlua.c \
                $(SRC)/src/decode.c \
                $(SRC)/src/encode.c \
                $(SRC)/src/dates.c

ifdef out
PREFIX       ?= $(out)
endif
ifdef PREFIX
BINDIR       ?= $(PREFIX)/bin
LUADIR       ?= $(PREFIX)/lua
LIBDIR       ?= $(PREFIX)/lib
endif

define FIX_SHEBANG
io.write(string.format("#!%s%cpackage.cpath = %q .. [[/?.so;]] .. package.cpath%c-- ", arg[1], 10, arg[2], 10))
endef

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

define newline


endef
define FIX_BEAR_RESULT
local input, tmp = "compile_commands.json", "compile_commands.tmp";
local infile = assert(io.open(input, "r"));
local outfile = assert(io.open(tmp, "w"));
for line in infile:lines() do
  if not line:find("-###", 1, true) then
    outfile:write(line, "\n");
  end
end
infile:close();
outfile:close();
assert(os.rename(tmp, input));
endef

BENCH_ITERS  ?= 100000

build: $(SRC)/src/*
	$(check_lua_incdir)
	@mkdir -p $(DESTDIR)
	$(CC) $(CFLAGS) -o $(DESTDIR)/tomlua.so $(SRCS)

bear:   # used to generate compile_commands.json, which editor tools such as clangd and ccls use
	$(check_lua_incdir)
	@$(BEAR) -- $(CC) -### $(CFLAGS) -o $(DESTDIR)/tomlua.so $(SRCS) > /dev/null 2>&1;
	@echo '$(subst $(newline), ,$(FIX_BEAR_RESULT))' | $(LUA) -;
	@echo "Created compile_commands.json";

test: $(SRC)/src/* $(TESTDIR)/*
	$(check_so_was_built)
	$(LUA) "$(TESTDIR)/test.lua" "$(DESTDIR)"

scratch: $(SRC)/src/* $(TESTDIR)/*
	$(check_so_was_built)
	$(LUA) "$(TESTDIR)/test.lua" "$(DESTDIR)" 1

bench: $(SRC)/src/* $(TESTDIR)/*
	$(check_so_was_built)
	$(LUA) "$(TESTDIR)/test.lua" "$(DESTDIR)" 2 $(BENCH_ITERS) $(SKIP_TOML_EDIT)

install: $(SRC)/lua/tomlua/meta.lua $(SRC)/bin/tomlua
ifdef LIBDIR
	$(check_so_was_built)
	@mkdir -p "$(LIBDIR)";
	@cp "$(DESTDIR)/tomlua.so" "$(LIBDIR)/";
	@echo "Installed library to $(LIBDIR)";
ifdef LUADIR
	@mkdir -p "$(LUADIR)/tomlua";
	@cp "$(SRC)/lua/tomlua/meta.lua" "$(LUADIR)/tomlua/";
	@echo "Installed type definitions to $(LUADIR)";
endif
ifdef BINDIR
	@mkdir -p "$(BINDIR)";
ifeq ($(filter /%,$(LUA)),)
	@echo '$(FIX_SHEBANG)' | $(LUA) - "/usr/bin/env $(LUA)" "$(abspath $(LIBDIR))" > "$(BINDIR)/tomlua"
else
	@echo '$(FIX_SHEBANG)' | $(LUA) - "$(LUA)" "$(abspath $(LIBDIR))" > "$(BINDIR)/tomlua"
endif
	@cat "$(SRC)/bin/tomlua" >> "$(BINDIR)/tomlua";
	@chmod +x "$(BINDIR)/tomlua";
	@echo "Installed binary to $(BINDIR)";
endif
else
	@echo "LIBDIR not set, skipping install"
endif

clean:
	rm -rf $(DESTDIR) compile_commands.json compile_commands.tmp build
