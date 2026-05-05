#include "./argus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int flag_cmp(const void *a, const void *b) {
    return strcmp(((const ArgusFlag *)a)->name, ((const ArgusFlag *)b)->name);
}

static ArgusFlag *find_flag(ArgusFlag *flags, const char *name) {
    for (ArgusFlag *f = flags; f && f->name; f++) {
        if (strcmp(f->name, name) == 0) return f;
    }
    return NULL;
}

static bool starts_with_dashdash(const char *s) {
    return s[0] == '-' && s[1] == '-';
}

static void print_help(const ArgusConfig *config, const char *prog_name) {
    printf("Usage: %s [OPTIONS] [-- %s]\n\n", prog_name,
           config->tail_usage_str ? config->tail_usage_str : "[ ... ]");
    puts("Options:");

    int count = 0;
    for (ArgusFlag *f = config->flags; f && f->name; f++) count++;

    bool has_help = find_flag(config->flags, "help") != NULL;
    int total = count + (has_help ? 0 : 1);

    ArgusFlag *sorted = malloc(total * sizeof(ArgusFlag));
    memcpy(sorted, config->flags, count * sizeof(ArgusFlag));

    if (!has_help) {
        sorted[count].name = "help";
        sorted[count].arg = ARGUS_ARG_NONE;
        sorted[count].desc = "Show this help message and exit";
        sorted[count].action = NULL;
    }

    qsort(sorted, (size_t)total, sizeof(ArgusFlag), flag_cmp);

    int pad = config->padding > 0 ? config->padding : 10;

    for (int i = 0; i < total; i++) {
        const char *arg_spec = "";
        if (sorted[i].arg == ARGUS_ARG_REQUIRED) arg_spec = " <value>";
        else if (sorted[i].arg == ARGUS_ARG_BOOL) arg_spec = " [true|false]";

        char namebuf[256];
        snprintf(namebuf, sizeof(namebuf), "%s%s", sorted[i].name, arg_spec);

        const char *desc = sorted[i].desc ? sorted[i].desc : "";
        const char *nl = strchr(desc, '\n');
        if (nl) {
            printf("  --%-*s %.*s\n", pad, namebuf, (int)(nl - desc), desc);
            desc = nl + 1;
            while (*desc) {
                const char *next_nl = strchr(desc, '\n');
                if (next_nl) {
                    printf("    %-*s %.*s\n", pad, "", (int)(next_nl - desc), desc);
                    desc = next_nl + 1;
                } else {
                    printf("    %-*s %s\n", pad, "", desc);
                    break;
                }
            }
        } else {
            printf("  --%-*s %s\n", pad, namebuf, desc);
        }
        putchar('\n');
    }

    free(sorted);
}

int argus_parse(ArgusConfig *config) {
    if (!config || !config->flags) return -1;

    const char *prog_name = config->argc > 0 ? config->argv[0] : "program";
    bool has_help = find_flag(config->flags, "help") != NULL;

    if (config->argc <= 1) {
        print_help(config, prog_name);
        return 1;
    }

    bool parse_end = false;
    for (int i = 1; i < config->argc; i++) {
        const char *v = config->argv[i];

        if (parse_end) {
            if (config->parse_end_action)
                config->parse_end_action(v, i, config->userdata);
        } else {
            if (starts_with_dashdash(v)) {
                if (v[2] == '\0') {
                    parse_end = true;
                } else {
                    const char *name = v + 2;

                    if (!has_help && strcmp(name, "help") == 0) {
                        print_help(config, prog_name);
                        return 1;
                    }

                    ArgusFlag *flag = find_flag(config->flags, name);
                    if (!flag) {
                        fprintf(stderr, "error: unknown flag --%s\n", name);
                        return -1;
                    }

                    bool has_arg = false;
                    const char *arg_val = NULL;

                    if (flag->arg == ARGUS_ARG_REQUIRED) {
                        if (i + 1 >= config->argc) {
                            fprintf(stderr, "error: flag --%s requires an argument\n", name);
                            return -1;
                        }
                        has_arg = true;
                        arg_val = config->argv[i + 1];
                        i++;
                    } else if (flag->arg == ARGUS_ARG_BOOL) {
                        if (i + 1 < config->argc) {
                            const char *next = config->argv[i + 1];
                            bool next_is_flag = starts_with_dashdash(next)
                                && strcmp(next, "--") != 0
                                && (find_flag(config->flags, next + 2)
                                    || (!has_help && strcmp(next + 2, "help") == 0));
                            if (!next_is_flag) {
                                has_arg = true;
                                arg_val = next;
                                i++;
                            }
                        }
                    }

                    if (flag->action) {
                        flag->action(has_arg, arg_val, config->userdata);
                    } else if (config->default_action) {
                        config->default_action(has_arg, name, arg_val, config->userdata);
                    }
                }
            } else {
                if (config->positional_action)
                    config->positional_action(v, i, config->userdata);
                else {
                    fprintf(stderr, "error: positional argument before --: %s\n", v);
                    return -1;
                }
            }
        }
    }

    return 0;
}
