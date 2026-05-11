#ifndef SRC_ARGUS_H_
#define SRC_ARGUS_H_

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ARGUS_ARG_NONE = 0,
    ARGUS_ARG_REQUIRED,
    ARGUS_ARG_BOOL,
} ArgusArgType;

typedef void (*argus_flag_action_fn)(bool has_arg, const char *val, void *userdata);
typedef void (*argus_default_action_fn)(bool has_arg, const char *name, const char *val, void *userdata);
typedef void (*argus_parse_end_fn)(const char *val, int index, void *userdata);
typedef void (*argus_positional_fn)(const char *val, int index, void *userdata);

typedef struct {
    const char *name;
    ArgusArgType arg;
    const char *desc;
    argus_flag_action_fn action;
} ArgusFlag;

typedef struct {
    int argc;
    char **argv;
    ArgusFlag *flags;
    int padding;
    const char *tail_usage_str;
    argus_default_action_fn default_action;
    argus_parse_end_fn parse_end_action;
    argus_positional_fn positional_action;
    void *userdata;
} ArgusConfig;

int argus_parse(ArgusConfig *config);

#ifdef __cplusplus
}
#endif

#endif  // SRC_ARGUS_H_
