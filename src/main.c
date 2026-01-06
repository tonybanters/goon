#include "goon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    fprintf(stderr, "usage: %s <command> [options]\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "commands:\n");
    fprintf(stderr, "  eval <file>     evaluate file and output JSON\n");
    fprintf(stderr, "  check <file>    validate syntax\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -p, --pretty    pretty print JSON output\n");
    fprintf(stderr, "  -h, --help      show this help\n");
    fprintf(stderr, "  -v, --version   show version\n");
}

static void print_version(void) {
    printf("goon %s\n", GOON_VERSION);
}

static int cmd_eval(const char *path, bool pretty) {
    Goon_Ctx *ctx = goon_create();
    if (!ctx) {
        fprintf(stderr, "error: failed to create context\n");
        return 1;
    }

    if (!goon_load_file(ctx, path)) {
        const Goon_Error *err = goon_get_error_info(ctx);
        if (err) {
            goon_error_print(err);
        } else {
            fprintf(stderr, "error: unknown error\n");
        }
        goon_destroy(ctx);
        return 1;
    }

    Goon_Value *result = goon_eval_result(ctx);
    char *json = pretty ? goon_to_json_pretty(result, 2) : goon_to_json(result);
    if (json) {
        printf("%s\n", json);
        free(json);
    }

    goon_destroy(ctx);
    return 0;
}

static int cmd_check(const char *path) {
    Goon_Ctx *ctx = goon_create();
    if (!ctx) {
        fprintf(stderr, "error: failed to create context\n");
        return 1;
    }

    if (!goon_load_file(ctx, path)) {
        const Goon_Error *err = goon_get_error_info(ctx);
        if (err) {
            goon_error_print(err);
        } else {
            fprintf(stderr, "error: unknown error\n");
        }
        goon_destroy(ctx);
        return 1;
    }

    goon_destroy(ctx);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(cmd, "-v") == 0 || strcmp(cmd, "--version") == 0) {
        print_version();
        return 0;
    }

    if (strcmp(cmd, "eval") == 0) {
        if (argc < 3) {
            fprintf(stderr, "error: eval requires a file argument\n");
            return 1;
        }
        bool pretty = false;
        const char *path = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pretty") == 0) {
                pretty = true;
            } else if (!path) {
                path = argv[i];
            }
        }
        if (!path) {
            fprintf(stderr, "error: eval requires a file argument\n");
            return 1;
        }
        return cmd_eval(path, pretty);
    }

    if (strcmp(cmd, "check") == 0) {
        if (argc < 3) {
            fprintf(stderr, "error: check requires a file argument\n");
            return 1;
        }
        return cmd_check(argv[2]);
    }

    fprintf(stderr, "error: unknown command '%s'\n", cmd);
    print_usage(argv[0]);
    return 1;
}
