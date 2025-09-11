// Copyright 2025 Birdee
#ifndef SRC_DATES_H_
#define SRC_DATES_H_

#include <lua.h>
#include <string.h>
#include <strings.h>
#include <lauxlib.h>
#include "./types.h"

static const uint8_t DATE_LENGTH = 10;
static const char *DATE_FIELD_NAMES[10] = {
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

typedef struct {
    TomlType toml_type;
    // e.g. 1979
    int year;
    // 1–12
    int month;
    // 1–31
    int day;
    // 0–23
    int hour;
    // 0–59
    int minute;
    // 0–59 (leap second 60 possible if you want to support it)
    int second;
    // fractional seconds, 0–999999
    // (millisecond precision is required; microsecond or nanosecond optional)
    int fractional;
    // UTC offset hours, e.g. -7 for -07:00
    int offset_hour;
    // UTC offset minutes, usually 0 but can be 30/45 for some zones
    int offset_minute;
} TomlDate;

static inline int string_2_date_field_idx(const char *str) {
    for (uint8_t i = 0; i < DATE_LENGTH; i++) {
        if (strcmp(str, DATE_FIELD_NAMES[i]) == 0) return i;
    }
    return -1;
}

typedef struct {
    int v;
    bool ok;
} date_result;

static inline date_result date_field_by_idx(TomlDate *date, int idx) {
    date_result value = {0};
    switch (idx) {
        case 0: value.v = date->toml_type; break;
        case 1: value.v = date->year; break;
        case 2: value.v = date->month; break;
        case 3: value.v = date->day; break;
        case 4: value.v = date->hour; break;
        case 5: value.v = date->minute; break;
        case 6: value.v = date->second; break;
        case 7: value.v = date->fractional; break;
        case 8: value.v = date->offset_hour; break;
        case 9: value.v = date->offset_minute; break;
        default: return value;
    }
    value.ok = true;
    return value;
}

static inline bool date_field_set_by_idx(TomlDate *date, int idx, int val) {
    switch (idx) {
        case 0: date->toml_type = val; break;
        case 1: date->year = val; break;
        case 2: date->month = val; break;
        case 3: date->day = val; break;
        case 4: date->hour = val; break;
        case 5: date->minute = val; break;
        case 6: date->second = val; break;
        case 7: date->fractional = val; break;
        case 8: date->offset_hour = val; break;
        case 9: date->offset_minute = val; break;
        default: return false;
    }
    return true;
}

bool parse_toml_date(str_iter *src, TomlDate *date);
// NOTE: CANNOT BE USED IN DECODE (uses luaL_error)
bool push_new_toml_date(lua_State *L, TomlDate date);
bool buf_push_toml_date(str_buf *buf, TomlDate *date);

#endif  // SRC_DATES_H_
