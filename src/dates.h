// Copyright 2025 Birdee
#ifndef SRC_DATES_H_
#define SRC_DATES_H_

#include <lua.h>
#include <lauxlib.h>
#include "./types.h"

typedef enum {
    TOMLDATE_TOML_TYPE,
    // e.g. 1979
    TOMLDATE_YEAR,
    // 1–12
    TOMLDATE_MONTH,
    // 1–31
    TOMLDATE_DAY,
    // 0–23
    TOMLDATE_HOUR,
    // 0–59
    TOMLDATE_MINUTE,
    // 0–59 (leap second 60 possible if you want to support it)
    TOMLDATE_SECOND,
    // fractional seconds, 0–999999
    // (millisecond precision is required; microsecond or nanosecond optional)
    TOMLDATE_FRACTIONAL,
    // UTC offset hours, e.g. -7 for -07:00
    TOMLDATE_OFFSET_HOUR,
    // UTC offset minutes, usually 0 but can be 30/45 for some zones
    TOMLDATE_OFFSET_MINUTE,
    TOMLDATE_DATE_LENGTH,
} TOMLDATE_FIELDS;
static const char *DATE_FIELD_NAMES[TOMLDATE_DATE_LENGTH] = {
    "toml_type",
    "year",
    "month",
    "day",
    "hour",
    "minute",
    "second",
    "fractional",
    "offset_hour",
    "offset_minute",
};
typedef int TomlDate[TOMLDATE_DATE_LENGTH];

static TOMLDATE_FIELDS string_2_date_field_idx(const char *str) {
    for (uint8_t i = 0; i < TOMLDATE_DATE_LENGTH; i++) {
        if (strcmp(str, DATE_FIELD_NAMES[i]) == 0) return i;
    }
    return TOMLDATE_DATE_LENGTH;
}

bool parse_toml_date(str_iter *src, TomlDate date);
bool push_new_toml_date(lua_State *L, TomlDate date);
bool buf_push_toml_date(str_buf *buf, TomlDate date);
// NOTE: for lua
int lnew_date(lua_State *L);

#endif  // SRC_DATES_H_
