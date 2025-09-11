#include <limits.h>
#include <lua.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <lauxlib.h>
#include "./types.h"
#include "./dates.h"

bool buf_push_toml_date(str_buf *buf, TomlDate *date) {
    char tmp[64];
    int n;

    switch (date->toml_type) {
        case TOML_LOCAL_DATE:
            n = snprintf(tmp, sizeof(tmp), "%04d-%02d-%02d",
                         date->year, date->month, date->day);
            if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
            return buf_push_str(buf, tmp, (size_t)n);

        case TOML_LOCAL_TIME:
            n = snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d",
                         date->hour, date->minute, date->second);
            if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
            if (!buf_push_str(buf, tmp, (size_t)n)) return false;

            if (date->fractional > 0) {
                n = snprintf(tmp, sizeof(tmp), ".%d", date->fractional);
                if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
                if (!buf_push_str(buf, tmp, (size_t)n)) return false;
            }
            return true;

        case TOML_LOCAL_DATETIME:
            n = snprintf(tmp, sizeof(tmp), "%04d-%02d-%02dT%02d:%02d:%02d",
                         date->year, date->month, date->day,
                         date->hour, date->minute, date->second);
            if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
            if (!buf_push_str(buf, tmp, (size_t)n)) return false;

            if (date->fractional > 0) {
                n = snprintf(tmp, sizeof(tmp), ".%d", date->fractional);
                if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
                if (!buf_push_str(buf, tmp, (size_t)n)) return false;
            }
            return true;

        case TOML_OFFSET_DATETIME:
            n = snprintf(tmp, sizeof(tmp), "%04d-%02d-%02dT%02d:%02d:%02d",
                         date->year, date->month, date->day,
                         date->hour, date->minute, date->second);
            if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
            if (!buf_push_str(buf, tmp, (size_t)n)) return false;

            if (date->fractional > 0) {
                n = snprintf(tmp, sizeof(tmp), ".%d", date->fractional);
                if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
                if (!buf_push_str(buf, tmp, (size_t)n)) return false;
            }

            if (date->offset_hour == 0 && date->offset_minute == 0) {
                if (!buf_push(buf, 'Z')) return false;
            } else {
                char sign = (date->offset_hour < 0 || date->offset_minute < 0) ? '-' : '+';
                int oh = date->offset_hour < 0 ? -date->offset_hour : date->offset_hour;
                int om = date->offset_minute < 0 ? -date->offset_minute : date->offset_minute;

                n = snprintf(tmp, sizeof(tmp), "%c%02d:%02d", sign, oh, om);
                if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
                if (!buf_push_str(buf, tmp, (size_t)n)) return false;
            }
            return true;
        default:
            return false;  // not a date type
    }
}

static inline bool char_isdigit(char c) {
    return '0' <= c && c <= '9';
}

static inline bool parse_number(str_iter *iter, int digits, int *out) {
    int val = 0;
    iter_result cur = iter_peek(iter);
    for (int i = 0; i < digits; i++) {
        int cur = iter_peek(iter).v;
        if (!char_isdigit(cur)) return false;
        val = val * 10 + (cur - '0');
        iter_skip(iter);
        cur = iter_peek(iter).v;
    }
    *out = val;
    return true;
}

static inline bool expect_char(str_iter *iter, char c) {
    iter_result r = iter_next(iter);
    return r.ok && r.v == c;
}

static inline bool parse_time(str_iter *src, TomlDate *date) {
    if (!parse_number(src, 2, &date->hour)) return false;
    if (!expect_char(src, ':')) return false;
    if (!parse_number(src, 2, &date->minute)) return false;
    if (!expect_char(src, ':')) return false;
    if (!parse_number(src, 2, &date->second)) return false;

    iter_result dot = iter_peek(src);
    if (dot.ok && dot.v == '.') {
        iter_skip(src);
        int val = 0;
        bool precision = 9;
        iter_result cur = iter_peek(src);
        while (char_isdigit(cur.v)) {
            if (precision > 0) {
                val = val * 10 + (cur.v - '0');
                precision--;
            }
            iter_skip(src);
            cur = iter_peek(src);
        }
        date->fractional = val;
    }
    return true;
}

bool parse_toml_date(str_iter *src, TomlDate *date) {
    // Reset struct
    *date = (TomlDate){0};

    // Parse YYYY-MM-DD
    if (!parse_number(src, 4, &date->year)) {
        if (!parse_time(src, date)) return false;
        date->toml_type = TOML_LOCAL_TIME;
        return true;
    }
    if (!expect_char(src, '-')) return false;
    if (!parse_number(src, 2, &date->month)) return false;
    if (!expect_char(src, '-')) return false;
    if (!parse_number(src, 2, &date->day)) return false;

    iter_result sep = iter_peek(src);
    if (!sep.ok) {
        date->toml_type = TOML_LOCAL_DATE;  // just a date, no time
        return true;
    }

    // Optional 'T' or space separator
    if (sep.v != 'T' && sep.v != ' ') {
        date->toml_type = TOML_LOCAL_DATE;  // just a date, no time
        return true;
    }
    iter_skip(src);  // consume separator

    if (!parse_time(src, date)) return false;

    // Optional time offset
    iter_result off = iter_peek(src);
    if (off.ok && (off.v == 'Z' || off.v == '+' || off.v == '-')) {
        if (off.v == 'Z') {
            date->offset_hour = 0;
            date->offset_minute = 0;
            iter_skip(src);
            date->toml_type = TOML_OFFSET_DATETIME;  // just a date, no time
            return true;
        } else {
            int sign = (off.v == '-') ? -1 : 1;
            iter_skip(src);
            int oh = 0, om = 0;
            if (!parse_number(src, 2, &oh)) return false;
            if (!expect_char(src, ':')) return false;
            if (!parse_number(src, 2, &om)) return false;
            date->offset_hour = sign * oh;
            date->offset_minute = sign * om;
            date->toml_type = TOML_OFFSET_DATETIME;  // just a date, no time
            return true;
        }
    }

    date->toml_type = TOML_LOCAL_DATETIME;
    return true;
}

static int ldate_tostring(lua_State *L) {
    TomlDate *date = (TomlDate *)lua_touserdata(L, 1);
    str_buf res = {0};
    if (!buf_push_toml_date(&res, date)) {
        free_str_buf(&res);
        return luaL_error(L, "could not format and store toml date object!");
    }
    lua_settop(L, 0);
    if (!push_buf_to_lua_string(L, &res)) {
        free_str_buf(&res);
        return luaL_error(L, "could not push string to lua stack!");
    }
    free_str_buf(&res);
    return 1;
}

static int ldate_index(lua_State *L) {
    TomlDate *date = (TomlDate *)lua_touserdata(L, 1);
    int ktype = lua_type(L, 2);
    int key = -1;
    switch (ktype) {
        case LUA_TNUMBER: {
            key = lua_tonumber(L, 2);
            if (key != (lua_Number)(lua_Integer)key) {
                lua_settop(L, 0);
                lua_pushnil(L);
                return 1;
            }
            key--;  // 1 to 0 arrays conversion
        } break;
        case LUA_TSTRING: {
            key = string_2_date_field_idx(lua_tostring(L, 2));
        } break;
        default: {
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }
    }
    date_result dr = date_field_by_idx(date, key);
    if (dr.ok) {
        lua_pushnumber(L, dr.v);
    } else {
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }
    return 1;
}

static int ldate_newindex(lua_State *L) {
    TomlDate *date = (TomlDate *)lua_touserdata(L, 1);
    int ktype = lua_type(L, 2);
    int key = -1;

    // Resolve key → index
    switch (ktype) {
        case LUA_TNUMBER: {
            lua_Number n = lua_tonumber(L, 2);
            if (n != (lua_Number)(lua_Integer)n) {
                return luaL_error(L, "invalid numeric index");
            }
            key = (int)n - 1; // 1-based to 0-based
        } break;
        case LUA_TSTRING: {
            key = string_2_date_field_idx(lua_tostring(L, 2));
        } break;
        default:
            return luaL_error(L, "invalid key type for TomlDate");
    }

    if (key < 0 || key >= DATE_LENGTH) {
        return luaL_error(L, "invalid field index");
    }

    if (!lua_isnumber(L, 3)) {
        return luaL_error(L, "value must be a number");
    }
    int value = (int)lua_tointeger(L, 3);

    // Actually set the field
    if (!date_field_set_by_idx(date, key, value)) {
        return luaL_error(L, "failed to set TomlDate field");
    }

    return 0; // __newindex returns nothing
}

static inline int ldate_iter(lua_State *L, bool is_ipairs) {
    TomlDate *date = (TomlDate *)lua_touserdata(L, 1);
    int ktype = lua_type(L, 2);
    int key = -1;
    if (is_ipairs && ktype == LUA_TNUMBER) {
        key = lua_tonumber(L, 2);
        lua_settop(L, 1);
        if (key != (lua_Number)(lua_Integer)key) {
            lua_pushnil(L);
            lua_pushnil(L);
            return 2;
        }
    } else if (!is_ipairs && ktype == LUA_TSTRING) {
        key = string_2_date_field_idx(lua_tostring(L, 2));
        key++;
        lua_settop(L, 1);
    } else if (ktype != LUA_TNIL) {
        lua_settop(L, 0);
        lua_pushnil(L);
        lua_pushnil(L);
        return 2;
    } else {
        key = 0;
        lua_settop(L, 1);
    }
    if (key < 0 || key >= DATE_LENGTH) {
        lua_settop(L, 0);
        lua_pushnil(L);
        lua_pushnil(L);
        return 2;
    }
    if (is_ipairs) {
        lua_pushinteger(L, key + 1);
    } else {
        lua_pushstring(L, DATE_FIELD_NAMES[key]);
    }
    date_result dr = date_field_by_idx(date, key);
    if (dr.ok) {
        lua_pushnumber(L, dr.v);
    } else {
        lua_settop(L, 0);
        lua_pushnil(L);
        lua_pushnil(L);
        return 2;
    }
    return 2;
}

static inline int ldate_ipairs_iter(lua_State *L) {
    return ldate_iter(L, true);
}

static inline int ldate_pairs_iter(lua_State *L) {
    return ldate_iter(L, false);
}

static int ldate_ipairs(lua_State *L) {
    lua_pushcfunction(L, ldate_ipairs_iter);  // iterator
    lua_pushvalue(L, 1);                      // state
    lua_pushinteger(L, 0);                    // initial key
    return 3;
}

static int ldate_pairs(lua_State *L) {
    lua_pushcfunction(L, ldate_pairs_iter);  // iterator
    lua_pushvalue(L, 1);                     // state
    lua_pushnil(L);                          // initial key
    return 3;
}

bool push_new_toml_date(lua_State *L, TomlDate date) {
    TomlDate *udate = (TomlDate *)lua_newuserdata(L, sizeof(TomlDate));
    *udate = date;
    if (luaL_newmetatable(L, "TomlDate")) {
        lua_pushcfunction(L, ldate_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, ldate_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, ldate_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, ldate_ipairs);
        lua_setfield(L, -2, "__ipairs");
        lua_pushcfunction(L, ldate_pairs);
        lua_setfield(L, -2, "__pairs");
        // TODO: implement __eq, __lt, and __le
    }
    lua_setmetatable(L, -2);
    return true;
}

// TODO: implement new_date function
// which should clone if given a date as an arg
// and should be able to read a table or list or varargs of integers corresponding to the TomlDate type
