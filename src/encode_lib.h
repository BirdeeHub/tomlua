// Copyright 2025 Birdee
#ifndef SRC_ENCODE_LIB_H_
#define SRC_ENCODE_LIB_H_

#include <lua.h>
#include <stddef.h>

void push_encode(lua_State *L, int opts_idx, int types_idx);

#endif  // SRC_ENCODE_LIB_H_
