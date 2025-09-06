#include <stdint.h>
#include "types.h"

// Convert a hex string to a codepoint
static uint32_t hex_to_codepoint(const char src[], int len) {
    uint32_t cp = 0;
    for (int i = 0; i < len; i++) {
        char c = src[i];
        cp <<= 4;
        if (c >= '0' && c <= '9') {
            cp |= c - '0';
        } else if (c >= 'A' && c <= 'F') {
            cp |= c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            cp |= c - 'a' + 10;
        } else {
            // invalid hex digit, treat as 0
            cp |= 0;
        }
    }
    return cp;
}

static bool utf8_encode(uint32_t cp, str_buf *dst) {
    char buf[4];
    size_t len = 0;

    if (cp <= 0x7F) {
        buf[0] = cp & 0x7F;
        len = 1;
    } else if (cp <= 0x7FF) {
        buf[0] = 0xC0 | ((cp >> 6) & 0x1F);
        buf[1] = 0x80 | (cp & 0x3F);
        len = 2;
    } else if (cp <= 0xFFFF) {
        buf[0] = 0xE0 | ((cp >> 12) & 0x0F);
        buf[1] = 0x80 | ((cp >> 6) & 0x3F);
        buf[2] = 0x80 | (cp & 0x3F);
        len = 3;
    } else if (cp <= 0x10FFFF) {
        buf[0] = 0xF0 | ((cp >> 18) & 0x07);
        buf[1] = 0x80 | ((cp >> 12) & 0x3F);
        buf[2] = 0x80 | ((cp >> 6) & 0x3F);
        buf[3] = 0x80 | (cp & 0x3F);
        len = 4;
    } else {
        // replacement character U+FFFD
        buf[0] = (char)0xEF;
        buf[1] = (char)0xBF;
        buf[2] = (char)0xBD;
        len = 3;
    }

    return buf_push_str(dst, buf, len);
}

static bool push_unicode(lua_State *L, str_buf *dst, char src[], int len) {
    for (size_t i = 0; i < len; i++) {
        if (!is_hex_char(src[i])) return set_err_upval(L, false, 33, "unexpected unicode specifier char");
    }
    uint32_t cp = hex_to_codepoint(src, len);
    if (!utf8_encode(cp, dst)) return set_err_upval(L, false, 3, "OOM");
    return true;
}

// pushes string to dst, advances pos
bool parse_basic_string(lua_State *L, str_buf *dst, str_iter *src) {
    while (iter_peek(src).ok) {
        iter_result current = iter_next(src);
        char c = current.v;
        iter_result nextres = iter_peek(src);
        if (c == '\\' && nextres.ok) {
            char next = iter_next(src).v;
            switch (next) {
                // Yes, using strdup on OOM error is stupid.
                // No, I do not want to find a way to track which of my errors are heap allocated for when I free them.
                case 'b': if (!buf_push(dst, '\b')) return set_err_upval(L, false, 3, "OOM"); break;
                case 't': if (!buf_push(dst, '\t')) return set_err_upval(L, false, 3, "OOM"); break;
                case 'n': if (!buf_push(dst, '\n')) return set_err_upval(L, false, 3, "OOM"); break;
                case 'f': if (!buf_push(dst, '\f')) return set_err_upval(L, false, 3, "OOM"); break;
                case 'r': if (!buf_push(dst, '\r')) return set_err_upval(L, false, 3, "OOM"); break;
                case '"': if (!buf_push(dst, '\"')) return set_err_upval(L, false, 3, "OOM"); break;
                case '\\': if (!buf_push(dst, '\\')) return set_err_upval(L, false, 3, "OOM"); break;
                // \uXXXX \UXXXXXXXX
                case 'u':
                case 'U': {
                    bool is_long = next == 'U';
                    int hex_len = is_long ? 8 : 4;
                    char escaped[8];
                    for (int i = 0; iter_peek(src).ok && i < hex_len; i++) {
                        escaped[i] = iter_next(src).v;
                    }
                    if (!push_unicode(L, dst, escaped, hex_len)) {
                        return false;
                    }
                } break;
                default:
                    if (!buf_push(dst, next)) return set_err_upval(L, false, 3, "OOM");
            }
        } else if (c == '\n' || c == '\r' && nextres.v == '\n') {
            return set_err_upval(L, false, 34, "basic strings are single-line only");
        } else if (c == '"') {
            return true;
        } else {
            if (!buf_push(dst, c)) return set_err_upval(L, false, 3, "OOM");
        }
    }
    return set_err_upval(L, false, 43, "end of content reached before end of string");
}

bool parse_multi_basic_string(lua_State *L, str_buf *dst, str_iter *src) {
    while (iter_peek(src).ok) {
        iter_result current = iter_next(src);
        char c = current.v;
        iter_result nextres = iter_peek(src);
        if (c == '\\' && nextres.ok) {
            char next = iter_next(src).v;
            switch (next) {
                // Yes, using strdup on OOM error is stupid.
                // No, I do not want to find a way to track which of my errors are heap allocated for when I free them.
                case 'b': if (!buf_push(dst, '\b')) return set_err_upval(L, false, 3, "OOM"); break;
                case 't': if (!buf_push(dst, '\t')) return set_err_upval(L, false, 3, "OOM"); break;
                case 'n': if (!buf_push(dst, '\n')) return set_err_upval(L, false, 3, "OOM"); break;
                case 'f': if (!buf_push(dst, '\f')) return set_err_upval(L, false, 3, "OOM"); break;
                case 'r': if (!buf_push(dst, '\r')) return set_err_upval(L, false, 3, "OOM"); break;
                case '"': if (!buf_push(dst, '\"')) return set_err_upval(L, false, 3, "OOM"); break;
                case '\\': if (!buf_push(dst, '\\')) return set_err_upval(L, false, 3, "OOM"); break;
                // \uXXXX \UXXXXXXXX
                case 'u':
                case 'U': {
                    bool is_long = next == 'U';
                    int hex_len = is_long ? 8 : 4;
                    char escaped[8];
                    for (int i = 0; iter_peek(src).ok && i < hex_len; i++) {
                        escaped[i] = iter_next(src).v;
                    }
                    if (!push_unicode(L, dst, escaped, hex_len)) {
                        return false;
                    }
                } break;
                default:
                    if (!buf_push(dst, next)) return set_err_upval(L, false, 3, "OOM");
            }
        } else if (c == '"' && iter_starts_with(src, "\"\"", 2)) {
            iter_skip_n(src, 2);
            return true;
        } else {
            if (!buf_push(dst, c)) return set_err_upval(L, false, 3, "OOM");
        }
    }
    return set_err_upval(L, false, 43, "end of content reached before end of string");
}

bool parse_literal_string(lua_State *L, str_buf *dst, str_iter *src) {
    while (iter_peek(src).ok) {
        iter_result current = iter_next(src);
        char c = current.v;
        iter_result nextres = iter_peek(src);
        if (c == '\n' || c == '\r' && nextres.v == '\n') {
            return set_err_upval(L, false, 36, "literal strings are single-line only");
        } else if (c == '\'') {
            return true;
        } else {
            if (!buf_push(dst, c)) return set_err_upval(L, false, 3, "OOM");
        }
    }
    return set_err_upval(L, false, 43, "end of content reached before end of string");
}

bool parse_multi_literal_string(lua_State *L, str_buf *dst, str_iter *src) {
    while (iter_peek(src).ok) {
        iter_result current = iter_next(src);
        char c = current.v;
        if (c == '\'' && iter_starts_with(src, "''", 2)) {
            iter_skip_n(src, 2);
            return true;
        } else {
            if (!buf_push(dst, c)) return set_err_upval(L, false, 3, "OOM");
        }
    }
    return set_err_upval(L, false, 43, "end of content reached before end of string");
}
