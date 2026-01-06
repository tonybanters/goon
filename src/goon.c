#define _POSIX_C_SOURCE 200809L
#include "goon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <libgen.h>

typedef enum {
    TOK_EOF,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_SEMICOLON,
    TOK_COMMA,
    TOK_EQUALS,
    TOK_COLON,
    TOK_QUESTION,
    TOK_SPREAD,
    TOK_DOT,
    TOK_INT,
    TOK_STRING,
    TOK_IDENT,
    TOK_TRUE,
    TOK_FALSE,
    TOK_LET,
    TOK_IF,
    TOK_THEN,
    TOK_ELSE,
    TOK_IMPORT,
} Token_Type;

typedef struct {
    Token_Type type;
    union {
        int64_t integer;
        char *string;
    } data;
} Token;

typedef struct {
    const char *src;
    size_t pos;
    size_t len;
    Token current;
    char *error;
} Lexer;

static void lexer_init(Lexer *lex, const char *src) {
    lex->src = src;
    lex->pos = 0;
    lex->len = strlen(src);
    lex->current.type = TOK_EOF;
    lex->current.data.string = NULL;
    lex->error = NULL;
}

static void lexer_skip_whitespace(Lexer *lex) {
    while (lex->pos < lex->len) {
        char c = lex->src[lex->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            lex->pos++;
        } else if (c == '/' && lex->pos + 1 < lex->len && lex->src[lex->pos + 1] == '/') {
            lex->pos += 2;
            while (lex->pos < lex->len && lex->src[lex->pos] != '\n') {
                lex->pos++;
            }
        } else if (c == '/' && lex->pos + 1 < lex->len && lex->src[lex->pos + 1] == '*') {
            lex->pos += 2;
            while (lex->pos + 1 < lex->len) {
                if (lex->src[lex->pos] == '*' && lex->src[lex->pos + 1] == '/') {
                    lex->pos += 2;
                    break;
                }
                lex->pos++;
            }
        } else {
            break;
        }
    }
}

static bool is_ident_start(char c) {
    return isalpha(c) || c == '_';
}

static bool is_ident_char(char c) {
    return isalnum(c) || c == '_';
}

static char *strdup_range(const char *start, size_t len) {
    char *s = malloc(len + 1);
    if (!s) return NULL;
    memcpy(s, start, len);
    s[len] = '\0';
    return s;
}

static bool lexer_next(Lexer *lex) {
    if (lex->current.type == TOK_STRING || lex->current.type == TOK_IDENT) {
        free(lex->current.data.string);
        lex->current.data.string = NULL;
    }

    lexer_skip_whitespace(lex);

    if (lex->pos >= lex->len) {
        lex->current.type = TOK_EOF;
        return true;
    }

    char c = lex->src[lex->pos];

    if (c == '{') { lex->current.type = TOK_LBRACE; lex->pos++; return true; }
    if (c == '}') { lex->current.type = TOK_RBRACE; lex->pos++; return true; }
    if (c == '[') { lex->current.type = TOK_LBRACKET; lex->pos++; return true; }
    if (c == ']') { lex->current.type = TOK_RBRACKET; lex->pos++; return true; }
    if (c == '(') { lex->current.type = TOK_LPAREN; lex->pos++; return true; }
    if (c == ')') { lex->current.type = TOK_RPAREN; lex->pos++; return true; }
    if (c == ';') { lex->current.type = TOK_SEMICOLON; lex->pos++; return true; }
    if (c == ',') { lex->current.type = TOK_COMMA; lex->pos++; return true; }
    if (c == '=') { lex->current.type = TOK_EQUALS; lex->pos++; return true; }
    if (c == ':') { lex->current.type = TOK_COLON; lex->pos++; return true; }
    if (c == '?') { lex->current.type = TOK_QUESTION; lex->pos++; return true; }

    if (c == '.' && lex->pos + 2 < lex->len &&
        lex->src[lex->pos + 1] == '.' && lex->src[lex->pos + 2] == '.') {
        lex->current.type = TOK_SPREAD;
        lex->pos += 3;
        return true;
    }

    if (c == '.') {
        lex->current.type = TOK_DOT;
        lex->pos++;
        return true;
    }

    if (c == '"') {
        lex->pos++;
        size_t buf_size = 256;
        size_t buf_len = 0;
        char *buf = malloc(buf_size);
        if (!buf) return false;

        while (lex->pos < lex->len && lex->src[lex->pos] != '"') {
            char ch = lex->src[lex->pos];
            if (ch == '\\' && lex->pos + 1 < lex->len) {
                lex->pos++;
                ch = lex->src[lex->pos];
                switch (ch) {
                    case 'n': ch = '\n'; break;
                    case 't': ch = '\t'; break;
                    case 'r': ch = '\r'; break;
                    case '\\': ch = '\\'; break;
                    case '"': ch = '"'; break;
                    case '$': ch = '$'; break;
                    default: break;
                }
            }
            if (buf_len + 1 >= buf_size) {
                buf_size *= 2;
                buf = realloc(buf, buf_size);
                if (!buf) return false;
            }
            buf[buf_len++] = ch;
            lex->pos++;
        }

        if (lex->pos >= lex->len) {
            free(buf);
            lex->error = strdup("unterminated string");
            return false;
        }

        lex->pos++;
        buf[buf_len] = '\0';
        lex->current.type = TOK_STRING;
        lex->current.data.string = buf;
        return true;
    }

    if (isdigit(c) || (c == '-' && lex->pos + 1 < lex->len && isdigit(lex->src[lex->pos + 1]))) {
        int sign = 1;
        if (c == '-') {
            sign = -1;
            lex->pos++;
        }
        int64_t val = 0;
        while (lex->pos < lex->len && isdigit(lex->src[lex->pos])) {
            val = val * 10 + (lex->src[lex->pos] - '0');
            lex->pos++;
        }
        lex->current.type = TOK_INT;
        lex->current.data.integer = val * sign;
        return true;
    }

    if (is_ident_start(c)) {
        size_t start = lex->pos;
        while (lex->pos < lex->len && is_ident_char(lex->src[lex->pos])) {
            lex->pos++;
        }
        char *ident = strdup_range(lex->src + start, lex->pos - start);

        if (strcmp(ident, "true") == 0) {
            free(ident);
            lex->current.type = TOK_TRUE;
            return true;
        }
        if (strcmp(ident, "false") == 0) {
            free(ident);
            lex->current.type = TOK_FALSE;
            return true;
        }
        if (strcmp(ident, "let") == 0) {
            free(ident);
            lex->current.type = TOK_LET;
            return true;
        }
        if (strcmp(ident, "if") == 0) {
            free(ident);
            lex->current.type = TOK_IF;
            return true;
        }
        if (strcmp(ident, "then") == 0) {
            free(ident);
            lex->current.type = TOK_THEN;
            return true;
        }
        if (strcmp(ident, "else") == 0) {
            free(ident);
            lex->current.type = TOK_ELSE;
            return true;
        }
        if (strcmp(ident, "import") == 0) {
            free(ident);
            lex->current.type = TOK_IMPORT;
            return true;
        }

        lex->current.type = TOK_IDENT;
        lex->current.data.string = ident;
        return true;
    }

    lex->error = strdup("unexpected character");
    return false;
}

static Goon_Value *alloc_value(Goon_Ctx *ctx) {
    Goon_Value *val = malloc(sizeof(Goon_Value));
    if (!val) return NULL;
    val->type = GOON_NIL;
    val->next_alloc = ctx->values;
    ctx->values = val;
    return val;
}

static Goon_Record_Field *alloc_field(Goon_Ctx *ctx) {
    Goon_Record_Field *field = malloc(sizeof(Goon_Record_Field));
    if (!field) return NULL;
    field->key = NULL;
    field->value = NULL;
    field->next = ctx->fields;
    ctx->fields = field;
    return field;
}

Goon_Value *goon_nil(Goon_Ctx *ctx) {
    Goon_Value *val = alloc_value(ctx);
    if (!val) return NULL;
    val->type = GOON_NIL;
    return val;
}

Goon_Value *goon_bool(Goon_Ctx *ctx, bool b) {
    Goon_Value *val = alloc_value(ctx);
    if (!val) return NULL;
    val->type = GOON_BOOL;
    val->data.boolean = b;
    return val;
}

Goon_Value *goon_int(Goon_Ctx *ctx, int64_t i) {
    Goon_Value *val = alloc_value(ctx);
    if (!val) return NULL;
    val->type = GOON_INT;
    val->data.integer = i;
    return val;
}

Goon_Value *goon_string(Goon_Ctx *ctx, const char *s) {
    Goon_Value *val = alloc_value(ctx);
    if (!val) return NULL;
    val->type = GOON_STRING;
    val->data.string = strdup(s);
    return val;
}

Goon_Value *goon_list(Goon_Ctx *ctx) {
    Goon_Value *val = alloc_value(ctx);
    if (!val) return NULL;
    val->type = GOON_LIST;
    val->data.list.items = NULL;
    val->data.list.len = 0;
    val->data.list.cap = 0;
    return val;
}

Goon_Value *goon_record(Goon_Ctx *ctx) {
    Goon_Value *val = alloc_value(ctx);
    if (!val) return NULL;
    val->type = GOON_RECORD;
    val->data.record.fields = NULL;
    return val;
}

bool goon_is_nil(Goon_Value *val) {
    return val == NULL || val->type == GOON_NIL;
}

bool goon_is_bool(Goon_Value *val) {
    return val != NULL && val->type == GOON_BOOL;
}

bool goon_is_int(Goon_Value *val) {
    return val != NULL && val->type == GOON_INT;
}

bool goon_is_string(Goon_Value *val) {
    return val != NULL && val->type == GOON_STRING;
}

bool goon_is_list(Goon_Value *val) {
    return val != NULL && val->type == GOON_LIST;
}

bool goon_is_record(Goon_Value *val) {
    return val != NULL && val->type == GOON_RECORD;
}

bool goon_to_bool(Goon_Value *val) {
    if (val == NULL) return false;
    if (val->type == GOON_BOOL) return val->data.boolean;
    if (val->type == GOON_NIL) return false;
    return true;
}

int64_t goon_to_int(Goon_Value *val) {
    if (val == NULL || val->type != GOON_INT) return 0;
    return val->data.integer;
}

const char *goon_to_string(Goon_Value *val) {
    if (val == NULL || val->type != GOON_STRING) return NULL;
    return val->data.string;
}

void goon_list_push(Goon_Ctx *ctx, Goon_Value *list, Goon_Value *item) {
    (void)ctx;
    if (!list || list->type != GOON_LIST) return;
    if (list->data.list.len >= list->data.list.cap) {
        size_t new_cap = list->data.list.cap == 0 ? 8 : list->data.list.cap * 2;
        Goon_Value **new_items = realloc(list->data.list.items, new_cap * sizeof(Goon_Value *));
        if (!new_items) return;
        list->data.list.items = new_items;
        list->data.list.cap = new_cap;
    }
    list->data.list.items[list->data.list.len++] = item;
}

size_t goon_list_len(Goon_Value *list) {
    if (!list || list->type != GOON_LIST) return 0;
    return list->data.list.len;
}

Goon_Value *goon_list_get(Goon_Value *list, size_t index) {
    if (!list || list->type != GOON_LIST) return NULL;
    if (index >= list->data.list.len) return NULL;
    return list->data.list.items[index];
}

void goon_record_set(Goon_Ctx *ctx, Goon_Value *record, const char *key, Goon_Value *value) {
    if (!record || record->type != GOON_RECORD) return;

    Goon_Record_Field *f = record->data.record.fields;
    while (f) {
        if (strcmp(f->key, key) == 0) {
            f->value = value;
            return;
        }
        f = f->next;
    }

    Goon_Record_Field *field = alloc_field(ctx);
    if (!field) return;
    field->key = strdup(key);
    field->value = value;
    field->next = record->data.record.fields;
    record->data.record.fields = field;
}

Goon_Value *goon_record_get(Goon_Value *record, const char *key) {
    if (!record || record->type != GOON_RECORD) return NULL;
    Goon_Record_Field *f = record->data.record.fields;
    while (f) {
        if (strcmp(f->key, key) == 0) {
            return f->value;
        }
        f = f->next;
    }
    return NULL;
}

Goon_Record_Field *goon_record_fields(Goon_Value *record) {
    if (!record || record->type != GOON_RECORD) return NULL;
    return record->data.record.fields;
}

static Goon_Value *lookup(Goon_Ctx *ctx, const char *name) {
    Goon_Binding *b = ctx->env;
    while (b) {
        if (strcmp(b->name, name) == 0) {
            return b->value;
        }
        b = b->next;
    }
    return NULL;
}

static void define(Goon_Ctx *ctx, const char *name, Goon_Value *value) {
    Goon_Binding *b = ctx->env;
    while (b) {
        if (strcmp(b->name, name) == 0) {
            b->value = value;
            return;
        }
        b = b->next;
    }
    b = malloc(sizeof(Goon_Binding));
    if (!b) return;
    b->name = strdup(name);
    b->value = value;
    b->next = ctx->env;
    ctx->env = b;
}

typedef struct {
    Goon_Ctx *ctx;
    Lexer *lex;
} Parser;

static Goon_Value *parse_expr(Parser *p);

static Goon_Value *interpolate_string(Goon_Ctx *ctx, const char *str) {
    size_t len = strlen(str);
    size_t buf_size = len * 2 + 1;
    char *buf = malloc(buf_size);
    if (!buf) return goon_string(ctx, str);

    size_t buf_len = 0;
    size_t i = 0;

    while (i < len) {
        if (str[i] == '$' && i + 1 < len && str[i + 1] == '{') {
            i += 2;
            size_t var_start = i;
            while (i < len && str[i] != '}') {
                i++;
            }
            if (i < len) {
                char *var_name = strdup_range(str + var_start, i - var_start);
                Goon_Value *val = lookup(ctx, var_name);
                free(var_name);

                if (val) {
                    const char *insert = NULL;
                    char num_buf[32];
                    if (val->type == GOON_STRING) {
                        insert = val->data.string;
                    } else if (val->type == GOON_INT) {
                        snprintf(num_buf, sizeof(num_buf), "%ld", val->data.integer);
                        insert = num_buf;
                    } else if (val->type == GOON_BOOL) {
                        insert = val->data.boolean ? "true" : "false";
                    }
                    if (insert) {
                        size_t insert_len = strlen(insert);
                        while (buf_len + insert_len >= buf_size) {
                            buf_size *= 2;
                            buf = realloc(buf, buf_size);
                        }
                        memcpy(buf + buf_len, insert, insert_len);
                        buf_len += insert_len;
                    }
                }
                i++;
            }
        } else {
            if (buf_len + 1 >= buf_size) {
                buf_size *= 2;
                buf = realloc(buf, buf_size);
            }
            buf[buf_len++] = str[i++];
        }
    }

    buf[buf_len] = '\0';
    Goon_Value *result = goon_string(ctx, buf);
    free(buf);
    return result;
}

static Goon_Value *parse_record(Parser *p) {
    Goon_Value *record = goon_record(p->ctx);

    if (!lexer_next(p->lex)) return NULL;

    while (p->lex->current.type != TOK_RBRACE && p->lex->current.type != TOK_EOF) {
        if (p->lex->current.type == TOK_SPREAD) {
            if (!lexer_next(p->lex)) return NULL;
            Goon_Value *spread_val = parse_expr(p);
            if (spread_val && spread_val->type == GOON_RECORD) {
                Goon_Record_Field *f = spread_val->data.record.fields;
                while (f) {
                    goon_record_set(p->ctx, record, f->key, f->value);
                    f = f->next;
                }
            }
            if (p->lex->current.type == TOK_COMMA) {
                if (!lexer_next(p->lex)) return NULL;
            } else if (p->lex->current.type == TOK_SEMICOLON) {
                if (!lexer_next(p->lex)) return NULL;
            }
            continue;
        }

        if (p->lex->current.type != TOK_IDENT) {
            p->lex->error = strdup("expected field name");
            return NULL;
        }

        char *key = strdup(p->lex->current.data.string);
        if (!lexer_next(p->lex)) { free(key); return NULL; }

        if (p->lex->current.type == TOK_COLON) {
            if (!lexer_next(p->lex)) { free(key); return NULL; }
            if (!lexer_next(p->lex)) { free(key); return NULL; }
        }

        if (p->lex->current.type != TOK_EQUALS) {
            p->lex->error = strdup("expected = after field name");
            free(key);
            return NULL;
        }

        if (!lexer_next(p->lex)) { free(key); return NULL; }

        Goon_Value *value = parse_expr(p);
        if (!value) { free(key); return NULL; }

        goon_record_set(p->ctx, record, key, value);
        free(key);

        if (p->lex->current.type == TOK_SEMICOLON) {
            if (!lexer_next(p->lex)) return NULL;
        } else if (p->lex->current.type == TOK_COMMA) {
            if (!lexer_next(p->lex)) return NULL;
        }
    }

    if (p->lex->current.type != TOK_RBRACE) {
        p->lex->error = strdup("expected }");
        return NULL;
    }

    if (!lexer_next(p->lex)) return NULL;
    return record;
}

static Goon_Value *parse_list(Parser *p) {
    Goon_Value *list = goon_list(p->ctx);

    if (!lexer_next(p->lex)) return NULL;

    while (p->lex->current.type != TOK_RBRACKET && p->lex->current.type != TOK_EOF) {
        if (p->lex->current.type == TOK_SPREAD) {
            if (!lexer_next(p->lex)) return NULL;
            Goon_Value *spread_val = parse_expr(p);
            if (spread_val && spread_val->type == GOON_LIST) {
                for (size_t i = 0; i < spread_val->data.list.len; i++) {
                    goon_list_push(p->ctx, list, spread_val->data.list.items[i]);
                }
            }
        } else {
            Goon_Value *item = parse_expr(p);
            if (!item) return NULL;
            goon_list_push(p->ctx, list, item);
        }

        if (p->lex->current.type == TOK_COMMA) {
            if (!lexer_next(p->lex)) return NULL;
        }
    }

    if (p->lex->current.type != TOK_RBRACKET) {
        p->lex->error = strdup("expected ]");
        return NULL;
    }

    if (!lexer_next(p->lex)) return NULL;
    return list;
}

static Goon_Value *parse_import(Parser *p) {
    if (!lexer_next(p->lex)) return NULL;

    if (p->lex->current.type != TOK_LPAREN) {
        p->lex->error = strdup("expected ( after import");
        return NULL;
    }

    if (!lexer_next(p->lex)) return NULL;

    if (p->lex->current.type != TOK_STRING) {
        p->lex->error = strdup("expected string path in import");
        return NULL;
    }

    char *path = strdup(p->lex->current.data.string);
    if (!lexer_next(p->lex)) { free(path); return NULL; }

    if (p->lex->current.type != TOK_RPAREN) {
        p->lex->error = strdup("expected ) after import path");
        free(path);
        return NULL;
    }

    if (!lexer_next(p->lex)) { free(path); return NULL; }

    char full_path[1024];
    if (path[0] == '.' && p->ctx->base_path) {
        char *base_copy = strdup(p->ctx->base_path);
        char *dir = dirname(base_copy);
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, path);
        free(base_copy);
    } else if (p->ctx->base_path && path[0] != '/') {
        char *base_copy = strdup(p->ctx->base_path);
        char *dir = dirname(base_copy);
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, path);
        free(base_copy);
    } else {
        snprintf(full_path, sizeof(full_path), "%s", path);
    }

    size_t plen = strlen(full_path);
    if (plen < 5 || strcmp(full_path + plen - 5, ".goon") != 0) {
        strncat(full_path, ".goon", sizeof(full_path) - plen - 1);
    }

    free(path);

    FILE *f = fopen(full_path, "r");
    if (!f) {
        p->lex->error = strdup("could not open import file");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = malloc(size + 1);
    if (!source) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(source, 1, size, f);
    source[read_size] = '\0';
    fclose(f);

    char *old_base = p->ctx->base_path;
    p->ctx->base_path = strdup(full_path);

    Lexer import_lex;
    lexer_init(&import_lex, source);
    Parser import_parser;
    import_parser.ctx = p->ctx;
    import_parser.lex = &import_lex;

    if (!lexer_next(&import_lex)) {
        free(source);
        free(p->ctx->base_path);
        p->ctx->base_path = old_base;
        return NULL;
    }

    Goon_Value *result = NULL;
    while (import_lex.current.type != TOK_EOF) {
        result = parse_expr(&import_parser);
        if (!result) break;
    }

    free(source);
    free(p->ctx->base_path);
    p->ctx->base_path = old_base;

    return result;
}

static Goon_Value *parse_call(Parser *p, const char *name) {
    Goon_Value *fn = lookup(p->ctx, name);

    if (!lexer_next(p->lex)) return NULL;

    Goon_Value *args[16];
    size_t argc = 0;

    while (p->lex->current.type != TOK_RPAREN && p->lex->current.type != TOK_EOF && argc < 16) {
        args[argc++] = parse_expr(p);
        if (p->lex->current.type == TOK_COMMA) {
            if (!lexer_next(p->lex)) return NULL;
        }
    }

    if (p->lex->current.type != TOK_RPAREN) {
        p->lex->error = strdup("expected )");
        return NULL;
    }

    if (!lexer_next(p->lex)) return NULL;

    if (fn && fn->type == GOON_BUILTIN) {
        return fn->data.builtin(p->ctx, args, argc);
    }

    return goon_nil(p->ctx);
}

static Goon_Value *parse_primary(Parser *p) {
    Token tok = p->lex->current;

    switch (tok.type) {
        case TOK_INT: {
            Goon_Value *val = goon_int(p->ctx, tok.data.integer);
            if (!lexer_next(p->lex)) return NULL;
            return val;
        }

        case TOK_STRING: {
            Goon_Value *val = interpolate_string(p->ctx, tok.data.string);
            if (!lexer_next(p->lex)) return NULL;
            return val;
        }

        case TOK_TRUE: {
            Goon_Value *val = goon_bool(p->ctx, true);
            if (!lexer_next(p->lex)) return NULL;
            return val;
        }

        case TOK_FALSE: {
            Goon_Value *val = goon_bool(p->ctx, false);
            if (!lexer_next(p->lex)) return NULL;
            return val;
        }

        case TOK_IDENT: {
            char *name = strdup(tok.data.string);
            if (!lexer_next(p->lex)) { free(name); return NULL; }

            if (p->lex->current.type == TOK_LPAREN) {
                Goon_Value *result = parse_call(p, name);
                free(name);
                return result;
            }

            if (p->lex->current.type == TOK_DOT) {
                Goon_Value *val = lookup(p->ctx, name);
                free(name);

                while (p->lex->current.type == TOK_DOT) {
                    if (!lexer_next(p->lex)) return NULL;
                    if (p->lex->current.type != TOK_IDENT) {
                        p->lex->error = strdup("expected field name after .");
                        return NULL;
                    }
                    char *field = p->lex->current.data.string;
                    val = goon_record_get(val, field);
                    if (!lexer_next(p->lex)) return NULL;
                }
                return val ? val : goon_nil(p->ctx);
            }

            Goon_Value *val = lookup(p->ctx, name);
            free(name);
            return val ? val : goon_nil(p->ctx);
        }

        case TOK_LBRACE:
            return parse_record(p);

        case TOK_LBRACKET:
            return parse_list(p);

        case TOK_IMPORT:
            return parse_import(p);

        case TOK_LPAREN: {
            if (!lexer_next(p->lex)) return NULL;
            Goon_Value *val = parse_expr(p);
            if (p->lex->current.type != TOK_RPAREN) {
                p->lex->error = strdup("expected )");
                return NULL;
            }
            if (!lexer_next(p->lex)) return NULL;
            return val;
        }

        default:
            return NULL;
    }
}

static Goon_Value *parse_expr(Parser *p) {
    if (p->lex->current.type == TOK_LET) {
        if (!lexer_next(p->lex)) return NULL;

        if (p->lex->current.type != TOK_IDENT) {
            p->lex->error = strdup("expected identifier after let");
            return NULL;
        }

        char *name = strdup(p->lex->current.data.string);
        if (!lexer_next(p->lex)) { free(name); return NULL; }

        if (p->lex->current.type == TOK_COLON) {
            if (!lexer_next(p->lex)) { free(name); return NULL; }
            if (!lexer_next(p->lex)) { free(name); return NULL; }
        }

        if (p->lex->current.type != TOK_EQUALS) {
            p->lex->error = strdup("expected = in let binding");
            free(name);
            return NULL;
        }

        if (!lexer_next(p->lex)) { free(name); return NULL; }

        Goon_Value *value = parse_expr(p);
        if (!value) { free(name); return NULL; }

        define(p->ctx, name, value);
        free(name);

        if (p->lex->current.type == TOK_SEMICOLON) {
            if (!lexer_next(p->lex)) return NULL;
        }

        return value;
    }

    if (p->lex->current.type == TOK_IF) {
        if (!lexer_next(p->lex)) return NULL;

        Goon_Value *cond = parse_expr(p);
        if (!cond) return NULL;

        if (p->lex->current.type != TOK_THEN) {
            p->lex->error = strdup("expected 'then' after if condition");
            return NULL;
        }

        if (!lexer_next(p->lex)) return NULL;

        Goon_Value *then_val = parse_expr(p);
        if (!then_val) return NULL;

        if (p->lex->current.type != TOK_ELSE) {
            p->lex->error = strdup("expected 'else' after then branch");
            return NULL;
        }

        if (!lexer_next(p->lex)) return NULL;

        Goon_Value *else_val = parse_expr(p);
        if (!else_val) return NULL;

        return goon_to_bool(cond) ? then_val : else_val;
    }

    Goon_Value *val = parse_primary(p);
    if (!val) return NULL;

    if (p->lex->current.type == TOK_QUESTION) {
        if (!lexer_next(p->lex)) return NULL;

        Goon_Value *then_val = parse_expr(p);
        if (!then_val) return NULL;

        if (p->lex->current.type != TOK_COLON) {
            p->lex->error = strdup("expected : in ternary");
            return NULL;
        }

        if (!lexer_next(p->lex)) return NULL;

        Goon_Value *else_val = parse_expr(p);
        if (!else_val) return NULL;

        return goon_to_bool(val) ? then_val : else_val;
    }

    return val;
}

Goon_Ctx *goon_create(void) {
    Goon_Ctx *ctx = malloc(sizeof(Goon_Ctx));
    if (!ctx) return NULL;
    ctx->env = NULL;
    ctx->values = NULL;
    ctx->fields = NULL;
    ctx->error = NULL;
    ctx->base_path = NULL;
    ctx->userdata = NULL;
    return ctx;
}

void goon_destroy(Goon_Ctx *ctx) {
    if (!ctx) return;

    Goon_Binding *b = ctx->env;
    while (b) {
        Goon_Binding *next = b->next;
        free(b->name);
        free(b);
        b = next;
    }

    Goon_Value *v = ctx->values;
    while (v) {
        Goon_Value *next = v->next_alloc;
        if (v->type == GOON_STRING && v->data.string) {
            free(v->data.string);
        } else if (v->type == GOON_LIST && v->data.list.items) {
            free(v->data.list.items);
        }
        free(v);
        v = next;
    }

    Goon_Record_Field *f = ctx->fields;
    while (f) {
        Goon_Record_Field *next = f->next;
        if (f->key) free(f->key);
        free(f);
        f = next;
    }

    if (ctx->error) free(ctx->error);
    if (ctx->base_path) free(ctx->base_path);
    free(ctx);
}

void goon_set_userdata(Goon_Ctx *ctx, void *userdata) {
    ctx->userdata = userdata;
}

void *goon_get_userdata(Goon_Ctx *ctx) {
    return ctx->userdata;
}

void goon_register(Goon_Ctx *ctx, const char *name, Goon_Builtin_Fn fn) {
    Goon_Value *val = alloc_value(ctx);
    if (!val) return;
    val->type = GOON_BUILTIN;
    val->data.builtin = fn;
    define(ctx, name, val);
}

static Goon_Value *last_result = NULL;

bool goon_load_string(Goon_Ctx *ctx, const char *source) {
    Lexer lex;
    lexer_init(&lex, source);

    Parser parser;
    parser.ctx = ctx;
    parser.lex = &lex;

    if (!lexer_next(&lex)) {
        if (ctx->error) free(ctx->error);
        ctx->error = lex.error;
        return false;
    }

    last_result = NULL;

    while (lex.current.type != TOK_EOF) {
        Goon_Value *expr = parse_expr(&parser);
        if (!expr) {
            if (lex.error) {
                if (ctx->error) free(ctx->error);
                ctx->error = lex.error;
            }
            return false;
        }
        last_result = expr;
    }

    return true;
}

bool goon_load_file(Goon_Ctx *ctx, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        if (ctx->error) free(ctx->error);
        ctx->error = strdup("could not open file");
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = malloc(size + 1);
    if (!source) {
        fclose(f);
        if (ctx->error) free(ctx->error);
        ctx->error = strdup("out of memory");
        return false;
    }

    size_t read_size = fread(source, 1, size, f);
    source[read_size] = '\0';
    fclose(f);

    ctx->base_path = strdup(path);

    bool result = goon_load_string(ctx, source);
    free(source);
    return result;
}

const char *goon_get_error(Goon_Ctx *ctx) {
    return ctx->error;
}

Goon_Value *goon_eval_result(Goon_Ctx *ctx) {
    (void)ctx;
    return last_result;
}

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} String_Builder;

static void sb_init(String_Builder *sb) {
    sb->buf = malloc(256);
    sb->len = 0;
    sb->cap = 256;
    if (sb->buf) sb->buf[0] = '\0';
}

static void sb_append(String_Builder *sb, const char *str) {
    if (!sb->buf) return;
    size_t add_len = strlen(str);
    while (sb->len + add_len + 1 > sb->cap) {
        sb->cap *= 2;
        sb->buf = realloc(sb->buf, sb->cap);
        if (!sb->buf) return;
    }
    memcpy(sb->buf + sb->len, str, add_len + 1);
    sb->len += add_len;
}

static void sb_append_char(String_Builder *sb, char c) {
    char tmp[2] = {c, '\0'};
    sb_append(sb, tmp);
}

static void json_escape_string(String_Builder *sb, const char *str) {
    sb_append_char(sb, '"');
    while (*str) {
        switch (*str) {
            case '"':  sb_append(sb, "\\\""); break;
            case '\\': sb_append(sb, "\\\\"); break;
            case '\n': sb_append(sb, "\\n"); break;
            case '\r': sb_append(sb, "\\r"); break;
            case '\t': sb_append(sb, "\\t"); break;
            default:   sb_append_char(sb, *str); break;
        }
        str++;
    }
    sb_append_char(sb, '"');
}

static void value_to_json(String_Builder *sb, Goon_Value *val, int indent, int depth);

static void append_indent(String_Builder *sb, int indent, int depth) {
    if (indent <= 0) return;
    for (int i = 0; i < indent * depth; i++) {
        sb_append_char(sb, ' ');
    }
}

static void value_to_json(String_Builder *sb, Goon_Value *val, int indent, int depth) {
    if (!val || val->type == GOON_NIL) {
        sb_append(sb, "null");
        return;
    }

    switch (val->type) {
        case GOON_BOOL:
            sb_append(sb, val->data.boolean ? "true" : "false");
            break;

        case GOON_INT: {
            char num[32];
            snprintf(num, sizeof(num), "%ld", val->data.integer);
            sb_append(sb, num);
            break;
        }

        case GOON_STRING:
            json_escape_string(sb, val->data.string);
            break;

        case GOON_LIST: {
            sb_append_char(sb, '[');
            if (indent > 0 && val->data.list.len > 0) sb_append_char(sb, '\n');
            for (size_t i = 0; i < val->data.list.len; i++) {
                if (indent > 0) append_indent(sb, indent, depth + 1);
                value_to_json(sb, val->data.list.items[i], indent, depth + 1);
                if (i < val->data.list.len - 1) sb_append_char(sb, ',');
                if (indent > 0) sb_append_char(sb, '\n');
            }
            if (indent > 0 && val->data.list.len > 0) append_indent(sb, indent, depth);
            sb_append_char(sb, ']');
            break;
        }

        case GOON_RECORD: {
            sb_append_char(sb, '{');
            Goon_Record_Field *f = val->data.record.fields;
            size_t count = 0;
            Goon_Record_Field *tmp = f;
            while (tmp) { count++; tmp = tmp->next; }
            if (indent > 0 && count > 0) sb_append_char(sb, '\n');
            size_t idx = 0;
            while (f) {
                if (indent > 0) append_indent(sb, indent, depth + 1);
                json_escape_string(sb, f->key);
                sb_append_char(sb, ':');
                if (indent > 0) sb_append_char(sb, ' ');
                value_to_json(sb, f->value, indent, depth + 1);
                if (f->next) sb_append_char(sb, ',');
                if (indent > 0) sb_append_char(sb, '\n');
                f = f->next;
                idx++;
            }
            if (indent > 0 && count > 0) append_indent(sb, indent, depth);
            sb_append_char(sb, '}');
            break;
        }

        default:
            sb_append(sb, "null");
            break;
    }
}

char *goon_to_json(Goon_Value *val) {
    String_Builder sb;
    sb_init(&sb);
    value_to_json(&sb, val, 0, 0);
    return sb.buf;
}

char *goon_to_json_pretty(Goon_Value *val, int indent) {
    String_Builder sb;
    sb_init(&sb);
    value_to_json(&sb, val, indent, 0);
    return sb.buf;
}
