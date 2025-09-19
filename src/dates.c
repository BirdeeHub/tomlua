#include <limits.h>
#include <math.h>
#include <lua.h>
#include <stdio.h>
#include <string.h>
#include <lauxlib.h>
#include "./types.h"
#include "./dates.h"

#define DATE_GET(d, field) ((d)[TOMLDATE_##field])

static inline void date_zero(TomlDate *date) {
    memset(date, 0, sizeof(TomlDate));
}
static inline int digits_in_int(int value) {
    int digits;
    if (value == 0) {
        digits = 1;
    } else {
        // Use log10 to get number of decimal digits
        digits = (int)floor(log10((double)value)) + 1;
    }
    return digits;
}

bool buf_push_toml_date(str_buf *buf, TomlDate *date) {
    char tmp[64];
    int n;

    int normed_frac = DATE_GET(*date, FRACTIONAL);
    {
        int digits = digits_in_int(normed_frac);
        while (digits < 6) {
            normed_frac *= 10;
            digits++;
        }
    }

    switch ((*date)[TOMLDATE_TOML_TYPE]) {
        case TOML_LOCAL_DATE:
            n = snprintf(tmp, sizeof(tmp), "%04d-%02d-%02d",
                         DATE_GET(*date, YEAR), DATE_GET(*date, MONTH), DATE_GET(*date, DAY));
            if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
            return buf_push_str(buf, tmp, (size_t)n);

        case TOML_LOCAL_TIME:
            n = snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d",
                         DATE_GET(*date, HOUR), DATE_GET(*date, MINUTE), DATE_GET(*date, SECOND));
            if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
            if (!buf_push_str(buf, tmp, (size_t)n)) return false;

            if (DATE_GET(*date, FRACTIONAL) > 0) {
                n = snprintf(tmp, sizeof(tmp), ".%d", normed_frac);
                if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
                if (!buf_push_str(buf, tmp, (size_t)n)) return false;
            }
            return true;

        case TOML_LOCAL_DATETIME:
            n = snprintf(tmp, sizeof(tmp), "%04d-%02d-%02dT%02d:%02d:%02d",
                         DATE_GET(*date, YEAR), DATE_GET(*date, MONTH), DATE_GET(*date, DAY),
                         DATE_GET(*date, HOUR), DATE_GET(*date, MINUTE), DATE_GET(*date, SECOND));
            if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
            if (!buf_push_str(buf, tmp, (size_t)n)) return false;

            if (DATE_GET(*date, FRACTIONAL) > 0) {
                n = snprintf(tmp, sizeof(tmp), ".%d", normed_frac);
                if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
                if (!buf_push_str(buf, tmp, (size_t)n)) return false;
            }
            return true;

        case TOML_OFFSET_DATETIME:
            n = snprintf(tmp, sizeof(tmp), "%04d-%02d-%02dT%02d:%02d:%02d",
                         DATE_GET(*date, YEAR), DATE_GET(*date, MONTH), DATE_GET(*date, DAY),
                         DATE_GET(*date, HOUR), DATE_GET(*date, MINUTE), DATE_GET(*date, SECOND));
            if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
            if (!buf_push_str(buf, tmp, (size_t)n)) return false;

            if (DATE_GET(*date, FRACTIONAL) > 0) {
                n = snprintf(tmp, sizeof(tmp), ".%d", normed_frac);
                if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
                if (!buf_push_str(buf, tmp, (size_t)n)) return false;
            }

            if (DATE_GET(*date, OFFSET_HOUR) == 0 && DATE_GET(*date, OFFSET_MINUTE) == 0) {
                if (!buf_push(buf, 'Z')) return false;
            } else {
                char sign = (DATE_GET(*date, OFFSET_HOUR) < 0 || DATE_GET(*date, OFFSET_MINUTE) < 0) ? '-' : '+';
                int oh = DATE_GET(*date, OFFSET_HOUR) < 0 ? -DATE_GET(*date, OFFSET_HOUR) : DATE_GET(*date, OFFSET_HOUR);
                int om = DATE_GET(*date, OFFSET_MINUTE) < 0 ? -DATE_GET(*date, OFFSET_MINUTE) : DATE_GET(*date, OFFSET_MINUTE);

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

static bool parse_number(str_iter *iter, int digits, int *out) {
    int val = 0;
    char cur = iter_peek(iter).v;
    for (int i = 0; i < digits; i++) {
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

static bool parse_time(str_iter *src, TomlDate *date) {
    if (!parse_number(src, 2, &DATE_GET(*date, HOUR))) return false;
    if (!expect_char(src, ':')) return false;
    if (!parse_number(src, 2, &DATE_GET(*date, MINUTE))) return false;
    if (!expect_char(src, ':')) return false;
    if (!parse_number(src, 2, &DATE_GET(*date, SECOND))) return false;

    iter_result dot = iter_peek(src);
    if (dot.ok && dot.v == '.') {
        iter_skip(src);
        int val = 0;
        uint8_t precision = 9;
        iter_result cur = iter_peek(src);
        while (char_isdigit(cur.v)) {
            if (precision > 0) {
                val = val * 10 + (cur.v - '0');
                precision--;
            }
            iter_skip(src);
            cur = iter_peek(src);
        }
        DATE_GET(*date, FRACTIONAL) = val;
    }
    return true;
}

bool parse_toml_date(str_iter *src, TomlDate *date) {
    // Reset struct
    date_zero(date);

    // Parse YYYY-MM-DD
    if (!parse_number(src, 4, &DATE_GET(*date, YEAR))) {
        if (!parse_time(src, date)) return false;
        DATE_GET(*date, TOML_TYPE) = TOML_LOCAL_TIME;
        return true;
    }
    if (!expect_char(src, '-')) return false;
    if (!parse_number(src, 2, &DATE_GET(*date, MONTH))) return false;
    if (!expect_char(src, '-')) return false;
    if (!parse_number(src, 2, &DATE_GET(*date, DAY))) return false;

    iter_result sep = iter_peek(src);
    if (!sep.ok) {
        DATE_GET(*date, TOML_TYPE) = TOML_LOCAL_DATE;  // just a date, no time
        return true;
    }

    // Optional 'T' or space separator
    if (sep.v != 'T' && sep.v != ' ') {
        DATE_GET(*date, TOML_TYPE) = TOML_LOCAL_DATE;  // just a date, no time
        return true;
    }
    iter_skip(src);  // consume separator

    if (!parse_time(src, date)) return false;

    // Optional time offset
    iter_result off = iter_peek(src);
    if (off.ok && (off.v == 'Z' || off.v == '+' || off.v == '-')) {
        if (off.v == 'Z') {
            DATE_GET(*date, OFFSET_HOUR) = 0;
            DATE_GET(*date, OFFSET_MINUTE) = 0;
            iter_skip(src);
            DATE_GET(*date, TOML_TYPE) = TOML_OFFSET_DATETIME;  // just a date, no time
            return true;
        } else {
            int sign = (off.v == '-') ? -1 : 1;
            iter_skip(src);
            int oh = 0, om = 0;
            if (!parse_number(src, 2, &oh)) return false;
            if (!expect_char(src, ':')) return false;
            if (!parse_number(src, 2, &om)) return false;
            DATE_GET(*date, OFFSET_HOUR) = sign * oh;
            DATE_GET(*date, OFFSET_MINUTE) = sign * om;
            DATE_GET(*date, TOML_TYPE) = TOML_OFFSET_DATETIME;  // just a date, no time
            return true;
        }
    }

    DATE_GET(*date, TOML_TYPE) = TOML_LOCAL_DATETIME;
    return true;
}

static const int DAYS_IN_MONTH[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

static inline bool is_leap_year(int year) {
    return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

static inline int days_in_month(int year, int month) {
    if (month == 2 && is_leap_year(year)) return 29;
    return DAYS_IN_MONTH[month-1];
}

static uint64_t days_since_year0(int year, int month, int day) {
    // Count all days in previous years
    uint64_t y = (uint64_t)year;
    uint64_t total_days = y * 365ULL;          // base days
    total_days += y / 4;                       // leap years every 4 years
    total_days -= y / 100;                     // subtract century years
    total_days += y / 400;                     // add back 400-year multiples

    // Add days for previous months in current year
    for (int m = 1; m < month; m++) {
        total_days += DAYS_IN_MONTH[m-1];
    }
    // Add one more day if past February and leap year
    if (month > 2 && is_leap_year(year)) total_days += 1;

    // Add days in current month
    total_days += (day - 1); // days are 1-based
    return total_days;
}

// Convert TomlDate to a uint64_t "timestamp" in microseconds since 0000-01-01T00:00:00 UTC
static uint64_t tomldate_to_utc_timestamp(const TomlDate *d) {
    int y = DATE_GET(*d, YEAR);
    int m = DATE_GET(*d, MONTH);
    int day = DATE_GET(*d, DAY);
    int hour = DATE_GET(*d, HOUR) - DATE_GET(*d, OFFSET_HOUR);
    int minute = DATE_GET(*d, MINUTE) - DATE_GET(*d, OFFSET_MINUTE);
    int second = DATE_GET(*d, SECOND);
    int frac = DATE_GET(*d, FRACTIONAL);

    // Normalize minutes/hours/days
    minute += hour * 60;
    hour = minute / 60;
    minute %= 60;
    if (minute < 0) { minute += 60; hour -= 1; }
    day += hour / 24;
    hour %= 24;
    if (hour < 0) { hour += 24; day -= 1; }

    // Normalize day/month/year
    while (day <= 0) {
        m -= 1;
        if (m <= 0) { m = 12; y -= 1; }
        day += days_in_month(y, m);
    }
    while (day > days_in_month(y, m)) {
        day -= days_in_month(y, m);
        m += 1;
        if (m > 12) { m = 1; y += 1; }
    }

    // Count total days since year 0
    uint64_t total_days = days_since_year0(y, m, day);

    uint64_t total_seconds = total_days * 86400ULL + hour * 3600ULL + minute * 60ULL + second;
    int digits = digits_in_int(frac);
    uint64_t microseconds = frac;
    while (digits < 6) { microseconds *= 10; digits++; } // scale to microseconds
    while (digits > 6) { microseconds /= 10; digits--; } // truncate if too many digits
    uint64_t timestamp = total_seconds * 1000000ULL + microseconds; // fractional assumed in microseconds
    return timestamp;
}

static void utc_timestamp_to_tomldate(uint64_t timestamp, TomlDate *date) {
    // Reset the date
    date_zero(date);
    DATE_GET(*date, TOML_TYPE) = TOML_LOCAL_DATETIME;

    // Split timestamp into seconds and microseconds
    uint64_t total_seconds = timestamp / 1000000ULL;
    int microseconds = timestamp % 1000000ULL;
    DATE_GET(*date, FRACTIONAL) = microseconds;

    // Extract hour, minute, second
    DATE_GET(*date, SECOND) = total_seconds % 60;
    total_seconds /= 60;
    DATE_GET(*date, MINUTE) = total_seconds % 60;
    total_seconds /= 60;
    DATE_GET(*date, HOUR) = total_seconds % 24;
    uint64_t total_days = total_seconds / 24;

    // Compute year
    int y = 0;
    while (1) {
        int days_in_year = is_leap_year(y) ? 366 : 365;
        if (total_days >= (uint64_t)days_in_year) {
            total_days -= days_in_year;
            y++;
        } else {
            break;
        }
    }
    DATE_GET(*date, YEAR) = y;

    // Compute month
    int m = 1;
    while (1) {
        int dim = days_in_month(y, m);
        if (total_days >= (uint64_t)dim) {
            total_days -= dim;
            m++;
        } else {
            break;
        }
    }
    DATE_GET(*date, MONTH) = m;

    // Remaining days
    DATE_GET(*date, DAY) = (int)total_days + 1; // days are 1-based

    // No offset by default
    DATE_GET(*date, OFFSET_HOUR) = 0;
    DATE_GET(*date, OFFSET_MINUTE) = 0;
}

static int compare_dates(const TomlDate *a, const TomlDate *b) {
    uint64_t ts_a = tomldate_to_utc_timestamp(a);
    uint64_t ts_b = tomldate_to_utc_timestamp(b);

    if (ts_a < ts_b) return -1;
    if (ts_a > ts_b) return 1;
    return 0;
}

// Equality: return true if timestamps are equal
static int ldate_eq(lua_State *L) {
    TomlDate *a = (TomlDate *)lua_touserdata(L, 1);
    TomlDate *b = (TomlDate *)lua_touserdata(L, 2);
    lua_pushboolean(L, compare_dates(a, b) == 0);
    return 1;
}

// Less than: return true if a < b
static int ldate_lt(lua_State *L) {
    TomlDate *a = (TomlDate *)lua_touserdata(L, 1);
    TomlDate *b = (TomlDate *)lua_touserdata(L, 2);
    lua_pushboolean(L, compare_dates(a, b) < 0);
    return 1;
}

// Less than or equal: return true if a <= b
static int ldate_le(lua_State *L) {
    TomlDate *a = (TomlDate *)lua_touserdata(L, 1);
    TomlDate *b = (TomlDate *)lua_touserdata(L, 2);
    lua_pushboolean(L, compare_dates(a, b) <= 0);
    return 1;
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
        case LUA_TNUMBER:
            key = lua_tonumber(L, 2);
            if (key != (lua_Number)(lua_Integer)key) {
                lua_settop(L, 0);
                lua_pushnil(L);
                return 1;
            }
            key--;  // 1 to 0 arrays conversion
            break;
        case LUA_TSTRING:
            key = string_2_date_field_idx(lua_tostring(L, 2));
            break;
        default:
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
    }
    if (0 <= key && key < TOMLDATE_DATE_LENGTH) {
        if (key == TOMLDATE_TOML_TYPE) {
            lua_pushstring(L, toml_type_names[DATE_GET(*date, TOML_TYPE)]);
        } else {
            lua_pushnumber(L, (*date)[key]);
        }
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
        case LUA_TNUMBER:
            key = lua_tonumber(L, 2);
            if (key != (lua_Number)(lua_Integer)key) {
                return luaL_error(L, "invalid numeric index");
            }
            key = (int)key - 1; // 1-based to 0-based
            break;
        case LUA_TSTRING:
            key = string_2_date_field_idx(lua_tostring(L, 2));
            break;
        default:
            return luaL_error(L, "invalid key type for TomlDate");
    }

    if (key < 0 || key >= TOMLDATE_DATE_LENGTH) {
        return luaL_error(L, "invalid field index");
    }

    int vtype = lua_type(L, 3);
    int value;
    if (key == TOMLDATE_TOML_TYPE && vtype == LUA_TSTRING) {
        value = toml_type_from_name(lua_tostring(L, 3));
    } else if (vtype == LUA_TNUMBER) {
        value = (int)lua_tointeger(L, 3);
    } else {
        return luaL_error(L, "value must be a number");
    }

    // Actually set the field
    (*date)[key] = value;

    return 0; // __newindex returns nothing
}

static int ldate_iter(lua_State *L, bool is_ipairs) {
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
    if (key < 0 || key >= TOMLDATE_DATE_LENGTH) {
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
    if (0 <= key && key < TOMLDATE_DATE_LENGTH) {
        if (key == TOMLDATE_TOML_TYPE) {
            lua_pushstring(L, toml_type_names[DATE_GET(*date, TOML_TYPE)]);
        } else {
            lua_pushnumber(L, (*date)[key]);
        }
    } else {
        lua_settop(L, 0);
        lua_pushnil(L);
        lua_pushnil(L);
        return 2;
    }
    return 2;
}

static int ldate_ipairs_iter(lua_State *L) {
    return ldate_iter(L, true);
}

static int ldate_pairs_iter(lua_State *L) {
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

// __call metamethod:
// if arg is a string, set the current TomlDate from the string
// if arg is a userdata of TomluaDate type, set the current TomlDate from the table
// if arg is a table, set the current TomlDate from the table
// table may be array or table type, as with __index and the iterators
// else, return date as an integer utc timestamp
static int ldate_call(lua_State *L) {
    TomlDate *date = (TomlDate *)lua_touserdata(L, 1);
    switch (lua_type(L, 2)) {
        case LUA_TNUMBER: {
            // if arg is an integer, set the current TomlDate from the timestamp
            utc_timestamp_to_tomldate(lua_tonumber(L, 2), date);
            lua_settop(L, 1);
            return 1;
        } break;
        case LUA_TSTRING: {
            // if arg is a string, set the current TomlDate from the string
            str_iter iter = lua_str_to_iter(L, 2);
            if (iter.buf == NULL || !parse_toml_date(&iter, date)) {
                return luaL_error(L, "invalid TOML date string");
            }
            lua_settop(L, 1);
            return 1;
        } break;
        case LUA_TTABLE: {
            date_zero(date);
            DATE_GET(*date, TOML_TYPE) = TOML_OFFSET_DATETIME;
            lua_pushnil(L);
            while (lua_next(L, 2) != 0) {
                int key_idx = -1;
                // handle key
                if (lua_type(L, -2) == LUA_TSTRING) {
                    key_idx = string_2_date_field_idx(lua_tostring(L, -2));
                } else if (lua_type(L, -2) == LUA_TNUMBER) {
                    lua_Number k = lua_tonumber(L, -2);
                    if (k == (lua_Number)(lua_Integer)k) {
                        key_idx = (int)k - 1; // convert 1-based to 0-based
                    }
                }
                int field_val = 0;
                // handle value
                int vtype = lua_type(L, -1);
                if (key_idx == TOMLDATE_TOML_TYPE && vtype == LUA_TSTRING) {
                    field_val = toml_type_from_name(lua_tostring(L, -1));
                } else if (vtype == LUA_TNUMBER) {
                    field_val = (int)lua_tointeger(L, -1);
                } else {
                    lua_pop(L, 1); // discard value
                    continue; // skip non-numeric values
                }
                if (key_idx >= 0 && key_idx < TOMLDATE_DATE_LENGTH) {
                    (*date)[key_idx] = field_val;
                }
                lua_pop(L, 1); // remove value, keep key for next iteration
            }
            lua_settop(L, 1);
            return 1;
        } break;
        case LUA_TUSERDATA:
            if(udata_is_of_type(L, 2, "TomluaDate")) {
                TomlDate *date2 = (TomlDate *)lua_touserdata(L, 2);
                memcpy(date, date2, sizeof(TomlDate));
                lua_settop(L, 1);
                return 1;
            }
        default: {
            // else, return date as an integer utc timestamp
            uint64_t ts = tomldate_to_utc_timestamp(date);
#if LUA_VERSION_NUM >= 503
            lua_pushinteger(L, (lua_Integer)ts);
#else
            lua_pushnumber(L, (lua_Number)ts);
#endif
            return 1;
        }
    }
}

bool push_new_toml_date(lua_State *L, TomlDate date) {
    TomlDate *udate = (TomlDate *)lua_newuserdata(L, sizeof(TomlDate));
    memcpy(*udate, date, sizeof(TomlDate));
    if (luaL_newmetatable(L, "TomluaDate")) {
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
        lua_pushcfunction(L, ldate_eq);
        lua_setfield(L, -2, "__eq");
        lua_pushcfunction(L, ldate_lt);
        lua_setfield(L, -2, "__lt");
        lua_pushcfunction(L, ldate_le);
        lua_setfield(L, -2, "__le");
        lua_pushcfunction(L, ldate_call);
        lua_setfield(L, -2, "__call");
    }
    lua_setmetatable(L, -2);
    return true;
}

// This function returns a new date object,
// and if provided any arguments, forwards them to __call or ldate_call on the object first
int lnew_date(lua_State *L) {
    TomlDate new_date = {0};
    DATE_GET(new_date, TOML_TYPE) = TOML_OFFSET_DATETIME;
    push_new_toml_date(L, new_date);
    lua_insert(L, 1);
    ldate_call(L);
    lua_settop(L, 1);
    return 1;
}
