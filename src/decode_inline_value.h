// Copyright 2025 Birdee
#ifndef SRC_DECODE_INLINE_VALUE_H_
#define SRC_DECODE_INLINE_VALUE_H_

#include <lua.h>
#include "./types.h"
#include "./opts.h"

// function is to recieve src iterator starting after the first `=`,
// and place 1 new item on the stack but otherwise leave the stack unchanged
bool decode_inline_value(lua_State *L, str_iter *src, str_buf *buf, const TomluaUserOpts opts, int erridx);

#endif  // SRC_DECODE_INLINE_VALUE_H_
