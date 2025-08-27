#include <stdint.h>
#include <string.h>
#include "str_buf.h"

static uint32_t hex_to_codepoint(const struct str_buf *src) {
    uint32_t cp = 0;
    for (size_t i = 0; i < src->len; i++) {
        char c = src->data[i];
        cp <<= 4; // multiply previous value by 16
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

static bool utf8_encode(uint32_t cp, struct str_buf *dst) {
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

static bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

static char *push_unicode(struct str_buf *dst, struct str_buf *src, bool is_long) {
    size_t expected_len = is_long ? 8 : 4;
    if (src->len != expected_len) return strdup("invalid unicode specifier length");
    for (size_t i = 0; i < src->len; i++) {
        if (!is_hex_char(src->data[i])) return strdup("unexpected unicode specifier char");
    }

    uint32_t cp = hex_to_codepoint(src);
    if (!utf8_encode(cp, dst)) return strdup("OOM");
    return NULL;
}

// pushes string to dst, advances pos
char *parse_basic_string(struct str_buf *dst, struct str_iter *src) {
    while (iter_peek(src).ok) {
        struct iter_result current = iter_next(src);
        char c = current.v;
        struct iter_result nextres = iter_peek(src);
        if (c == '\\' && nextres.ok) {
            char next = iter_next(src).v;
            switch (next) {
                // Yes, using strdup on OOM error is stupid.
                // No, I do not want to find a way to track which of my errors are heap allocated for when I free them.
                case 'b': if (!buf_push(dst, '\b')) return strdup("OOM"); break;
                case 't': if (!buf_push(dst, '\t')) return strdup("OOM"); break;
                case 'n': if (!buf_push(dst, '\n')) return strdup("OOM"); break;
                case 'f': if (!buf_push(dst, '\f')) return strdup("OOM"); break;
                case 'r': if (!buf_push(dst, '\r')) return strdup("OOM"); break;
                case '"': if (!buf_push(dst, '\"')) return strdup("OOM"); break;
                case '\\': if (!buf_push(dst, '\\')) return strdup("OOM"); break;
                // \uXXXX \UXXXXXXXX
                case 'u':
                case 'U': {
                    struct str_buf escaped = new_str_buf();
                    bool is_long = next == 'U';
                    int hex_len = is_long ? 8 : 4;
                    for (int i = 0; iter_peek(src).ok && i < hex_len; i++) {
                        if (!buf_push(&escaped, iter_next(src).v)) {
                            free_str_buf(&escaped);
                            return strdup("OOM");
                        }
                    }
                    char *err = push_unicode(dst, &escaped, is_long);
                    free_str_buf(&escaped);
                    if (err != NULL) return err;
                } break;
                default:
                    if (!buf_push(dst, next)) return strdup("OOM");
            }
        } else if (c == '\n' || c == '\r' && nextres.v == '\n') {
            return strdup("basic strings are single-line only");
        } else if (c == '"') {
            return NULL;
        } else {
            if (!buf_push(dst, c)) return strdup("OOM");
        }
    }
    return strdup("end of content reached before end of string");
}

char *parse_multi_basic_string(struct str_buf *dst, struct str_iter *src) {
    while (iter_peek(src).ok) {
        struct iter_result current = iter_next(src);
        char c = current.v;
        struct iter_result nextres = iter_peek(src);
        if (c == '\\' && nextres.ok) {
            char next = iter_next(src).v;
            switch (next) {
                // Yes, using strdup on OOM error is stupid.
                // No, I do not want to find a way to track which of my errors are heap allocated for when I free them.
                case 'b': if (!buf_push(dst, '\b')) return strdup("OOM"); break;
                case 't': if (!buf_push(dst, '\t')) return strdup("OOM"); break;
                case 'n': if (!buf_push(dst, '\n')) return strdup("OOM"); break;
                case 'f': if (!buf_push(dst, '\f')) return strdup("OOM"); break;
                case 'r': if (!buf_push(dst, '\r')) return strdup("OOM"); break;
                case '"': if (!buf_push(dst, '\"')) return strdup("OOM"); break;
                case '\\': if (!buf_push(dst, '\\')) return strdup("OOM"); break;
                // \uXXXX \UXXXXXXXX
                case 'u':
                case 'U': {
                    struct str_buf escaped = new_str_buf();
                    bool is_long = next == 'U';
                    int hex_len = is_long ? 8 : 4;
                    for (int i = 0; iter_peek(src).ok && i < hex_len; i++) {
                        if (!buf_push(&escaped, iter_next(src).v)) {
                            free_str_buf(&escaped);
                            return strdup("OOM");
                        }
                    }
                    char *err = push_unicode(dst, &escaped, is_long);
                    free_str_buf(&escaped);
                    if (err != NULL) return err;
                } break;
                default:
                    if (!buf_push(dst, next)) return strdup("OOM");
            }
        } else if (c == '"' && iter_starts_with(src, "\"\"", 2)) {
            iter_next(src);
            iter_next(src);
            return NULL;
        } else {
            if (!buf_push(dst, c)) return strdup("OOM");
        }
    }
    return strdup("end of content reached before end of string");
}

char *parse_literal_string(struct str_buf *dst, struct str_iter *src) {
    while (iter_peek(src).ok) {
        struct iter_result current = iter_next(src);
        char c = current.v;
        struct iter_result nextres = iter_peek(src);
        if (c == '\n' || c == '\r' && nextres.v == '\n') {
            return strdup("literal strings are single-line only");
        } else if (c == '\'') {
            return NULL;
        } else {
            if (!buf_push(dst, c)) return strdup("OOM");
        }
    }
    return strdup("end of content reached before end of string");
}

char *parse_multi_literal_string(struct str_buf *dst, struct str_iter *src) {
    while (iter_peek(src).ok) {
        struct iter_result current = iter_next(src);
        char c = current.v;
        if (c == '\'' && iter_starts_with(src, "''", 2)) {
            iter_next(src);
            iter_next(src);
            return NULL;
        } else {
            if (!buf_push(dst, c)) return strdup("OOM");
        }
    }
    return strdup("end of content reached before end of string");
}
