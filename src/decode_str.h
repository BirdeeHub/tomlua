// Copyright 2025 Birdee
#ifndef SRC_DECODE_STR_H_
#define SRC_DECODE_STR_H_

#include "./types.h"

bool parse_basic_string(lua_State *L, str_buf *dst, str_iter *src, int erridx);
bool parse_multi_basic_string(lua_State *L, str_buf *dst, str_iter *src, int erridx);
bool parse_literal_string(lua_State *L, str_buf *dst, str_iter *src, int erridx);
bool parse_multi_literal_string(lua_State *L, str_buf *dst, str_iter *src, int erridx);

#endif  // SRC_DECODE_STR_H_
