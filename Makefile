ifndef LUA_INCDIR
$(error LUA_INCDIR header files not provided. Please pass or export LUA_INCDIR=/path/to/lua/include)
endif
CC      ?= gcc
CFLAGS  = -O3 -fPIC -flto -fomit-frame-pointer -finline-functions -shared -Wl,-s
DESTDIR  ?= ./lib
OUT     = $(DESTDIR)/tomlua.so
SRCS    = ./src/tomlua.c ./src/parse_str.c ./src/parse_keys.c ./src/parse.c
INCLUDES = -I"$(LUA_INCDIR)"

all: build

build: $(SRCS)
	@mkdir -p $(DESTDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUT) $(SRCS)

bear: clean
	@mkdir -p $(DESTDIR)
	bear -- $(CC) $(CFLAGS) $(INCLUDES) -o $(OUT) $(SRCS)

clean:
	rm -rf $(DESTDIR) compile_commands.json
