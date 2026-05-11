src             ?= .
SRC             ?= $(src)
DESTDIR         ?= ./build
ifdef LUA_BINDIR
LUA             := $(if $(wildcard $(LUA_BINDIR)/luajit),$(LUA_BINDIR)/luajit,$(if $(wildcard $(LUA_BINDIR)/lua),$(LUA_BINDIR)/lua,lua))
else
ifdef LUA_DIR
LUA             := $(if $(wildcard $(LUA_DIR)/bin/luajit),$(LUA_DIR)/bin/luajit,$(if $(wildcard $(LUA_DIR)/bin/lua),$(LUA_DIR)/bin/lua,lua))
else
LUA             ?= lua
endif
endif
BEAR            ?= bear

ifdef out
PREFIX          ?= $(out)
endif
ifdef OUT
PREFIX          ?= $(OUT)
endif
ifdef PREFIX
BINDIR          ?= $(PREFIX)/bin
LUADIR          ?= $(PREFIX)/lua
LIBDIR          ?= $(PREFIX)/lib
endif

CC              ?= gcc
LIBFLAG         ?= -shared
CFLAGS          ?= -fPIC -x c -O3 -flto -Winline
LDFLAGS         ?= -Wl,-s

LUA_VERSION     ?= 5.1
LUA_NAME        ?= $(notdir $(LUA))
ifeq ($(LUA_NAME),luajit)
LUALIB          ?= $(LUA_NAME)-$(LUA_VERSION)
else
LUALIB          ?= $(LUA_NAME)
endif
ifeq ($(strip $(LUALIB)),)
ifeq ($(LUA_NAME),luajit)
override LUALIB := $(LUA_NAME)-$(LUA_VERSION)
else
override LUALIB := $(LUA_NAME)
endif
endif

ifdef LUA_INC
LUA_INCDIR      ?= $(LUA_INC)
endif
ifdef LUA_DIR
LUA_LIBDIR      ?= $(LUA_DIR)/lib
LUA_INCDIR      ?= $(LUA_DIR)/include
endif
ifeq ($(strip $(LUA_LIBDIR)),)
ifdef LUA_DIR
override LUA_LIBDIR := $(LUA_DIR)/lib
endif
endif

BINFLAG := -lm
ifneq ($(strip $(LUA_LIBDIR)),)
BINFLAG         += -L$(LUA_LIBDIR)
endif
ifdef LUALIB
BINFLAG         += -l$(LUALIB)
endif

ifneq ($(strip $(LUA_LIBDIR)$(LUALIB)),)
MAKE_BINARY     := 1
endif

CFLAGS          += -I$(LUA_INCDIR)

SRC             := $(abspath $(SRC))
TESTDIR         := $(SRC)/tests
SRCS            := $(SRC)/src/tomlua.c \
                  $(SRC)/src/decode.c \
                  $(SRC)/src/encode.c \
                  $(SRC)/src/env.c \
                  $(SRC)/src/dates.c

CLI_SRCS        := $(SRC)/src/tomlua_cli.c \
                  $(SRC)/src/argus.c

BUILD_DIR       ?= $(DESTDIR)
BUILD_DIR       := $(abspath $(DESTDIR))
LIB_BUILD_DIR   ?= $(BUILD_DIR)/lib
BIN_BUILD_DIR   ?= $(BUILD_DIR)/bin

check_lua_incdir = \
	@if [ -z "$(LUA_INCDIR)" ]; then \
		echo "Error: LUA_INCDIR not set. Please pass or export LUA_INCDIR=/path/to/lua/include"; \
		false; \
	fi

check_so_was_built = \
	@if [ ! -f "$(LIB_BUILD_DIR)/tomlua.so" ]; then \
		echo "Error: $(LIB_BUILD_DIR)/tomlua.so not built. Run make build first."; \
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
	@mkdir -p $(LIB_BUILD_DIR)
	$(CC) $(LIBFLAG) $(LDFLAGS) $(CFLAGS) -o $(LIB_BUILD_DIR)/tomlua.so $(SRCS)
ifdef MAKE_BINARY
	@mkdir -p $(BIN_BUILD_DIR)
	-$(CC) $(CFLAGS) $(LDFLAGS) $(BINFLAG) -o $(BIN_BUILD_DIR)/tomlua $(SRCS) $(CLI_SRCS)
endif

bear:   # used to generate compile_commands.json, which editor tools such as clangd and ccls use
	$(check_lua_incdir)
	@$(BEAR) -- $(CC) -### $(BINFLAG) $(CFLAGS) $(LDFLAGS) -o $(BIN_BUILD_DIR)/tomlua $(SRCS) $(CLI_SRCS) > /dev/null 2>&1;
	@echo '$(subst $(newline), ,$(FIX_BEAR_RESULT))' | $(LUA) -;
	@echo "Created compile_commands.json";

test: $(SRC)/src/* $(TESTDIR)/*
	$(check_so_was_built)
	$(LUA) "$(TESTDIR)/test.lua" "$(LIB_BUILD_DIR)"

scratch: $(SRC)/src/* $(TESTDIR)/* build
	$(LUA) "$(TESTDIR)/test.lua" "$(LIB_BUILD_DIR)" 1

bench: $(SRC)/src/* $(TESTDIR)/* build
	$(LUA) "$(TESTDIR)/test.lua" "$(LIB_BUILD_DIR)" 2 $(BENCH_ITERS) $(SKIP_TOML_EDIT)

install: $(SRC)/lua/tomlua/meta.lua
ifdef LIBDIR
	$(check_so_was_built)
	@mkdir -p "$(LIBDIR)";
	@cp "$(LIB_BUILD_DIR)/tomlua.so" "$(LIBDIR)/";
	@echo "Installed library to $(LIBDIR)";
ifdef LUADIR
	@mkdir -p "$(LUADIR)/tomlua";
	@cp "$(SRC)/lua/tomlua/meta.lua" "$(LUADIR)/tomlua/";
	@echo "Installed type definitions to $(LUADIR)";
endif
ifdef BINDIR
ifeq ($(if $(wildcard $(BIN_BUILD_DIR)/tomlua),1,0),1)
	@mkdir -p "$(BINDIR)";
	@cp "$(BIN_BUILD_DIR)/tomlua" "$(BINDIR)";
	@echo "Installed binary to $(BINDIR)";
endif
endif
else
	@echo "LIBDIR not set, skipping install"
endif

clean:
	rm -rf $(BUILD_DIR) compile_commands.json compile_commands.tmp
