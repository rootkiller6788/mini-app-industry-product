#include "cli_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
const char* flag_type_str(FlagType t) {
    switch (t) {
    case FLAG_BOOL:   return "bool";
    case FLAG_STRING: return "string";
    case FLAG_INT:    return "int";
    case FLAG_FLOAT:  return "float";
    case FLAG_COUNT:  return "count";
    default:          return "???";
    }
}

CLIParser* cli_create(const char *name, const char *desc) {
    CLIParser *cp = (CLIParser*)calloc(1, sizeof(CLIParser));
    if (!cp) return NULL;
    if (name) snprintf(cp->root.name, sizeof(cp->root.name), "%s", name);
    if (desc) snprintf(cp->root.description, sizeof(cp->root.description), "%s", desc);
    cp->root.flag_count = 0;
    cp->root.subcmd_count = 0;
    cp->current = &cp->root;
    return cp;
}

void cli_destroy(CLIParser *cp) { free(cp); }

int cli_add_flag(CLICommand *cmd, const char *name, char short_name,
                  FlagType t, const char *help) {
    if (!cmd || !name || cmd->flag_count >= MAX_ARGS) return -1;
    Flag *f = &cmd->flags[cmd->flag_count];
    memset(f, 0, sizeof(Flag));
    snprintf(f->name, sizeof(f->name), "%s", name);
    f->short_name = short_name;
    f->type = t;
    if (help) snprintf(f->help, sizeof(f->help), "%s", help);
    f->default_bool = false;
    f->default_int = 0;
    f->default_float = 0.0;
    f->value.bool_val = false;
    cmd->flag_count++;
    return cmd->flag_count - 1;
}

int cli_add_subcommand(CLICommand *parent, const char *name,
                        const char *desc, int (*handler)(CLICommand*, int, char**)) {
    if (!parent || !name || parent->subcmd_count >= MAX_SUBCMDS) return -1;
    CLICommand *sub = (CLICommand*)calloc(1, sizeof(CLICommand));
    if (!sub) return -1;
    snprintf(sub->name, sizeof(sub->name), "%s", name);
    if (desc) snprintf(sub->description, sizeof(sub->description), "%s", desc);
    sub->handler = handler;
    sub->flag_count = 0;
    sub->subcmd_count = 0;
    parent->subcommands[parent->subcmd_count] = sub;
    parent->subcmd_count++;
    return parent->subcmd_count - 1;
}

static Flag* find_flag(CLICommand *cmd, const char *name) {
    for (int i = 0; i < cmd->flag_count; i++) {
        if (strcmp(cmd->flags[i].name, name) == 0) return &cmd->flags[i];
        if (cmd->flags[i].short_name && name[0] == cmd->flags[i].short_name && name[1] == 0)
            return &cmd->flags[i];
    }
    return NULL;
}

static int parse_value(Flag *f, const char *val) {
    if (!val) return -1;
    switch (f->type) {
    case FLAG_STRING:
        snprintf(f->value.str_val, sizeof(f->value.str_val), "%s", val);
        break;
    case FLAG_INT:
        f->value.int_val = (int64_t)strtoll(val, NULL, 10);
        break;
    case FLAG_FLOAT:
        f->value.float_val = strtod(val, NULL);
        break;
    case FLAG_BOOL:
        f->value.bool_val = (strcasecmp(val, "true") == 0 ||
                             strcasecmp(val, "1") == 0 ||
                             strcasecmp(val, "yes") == 0);
        break;
    case FLAG_COUNT:
        f->value.count_val++;
        break;
    }
    f->parsed = true;
    return 0;
}

int cli_parse(CLIParser *cp, int argc, char **argv) {
    if (!cp || !argv) return -1;
    cp->arg_count = 0;
    CLICommand *cmd = &cp->root;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (arg[0] == '-' && arg[1] == '-') {
            char *name = arg + 2;
            char *eq = strchr(name, '=');
            char *val = NULL;
            if (eq) { *eq = '\0'; val = eq + 1; }

            Flag *f = find_flag(cmd, name);
            if (f) {
                if (f->type == FLAG_BOOL) {
                    f->value.bool_val = true;
                    f->parsed = true;
                } else if (val) {
                    parse_value(f, val);
                } else if (i + 1 < argc && argv[i+1][0] != '-') {
                    parse_value(f, argv[++i]);
                }
            }
        } else if (arg[0] == '-') {
            char short_name = arg[1];
            Flag *f = find_flag(cmd, (char[]){short_name, 0});
            if (f) {
                if (f->type == FLAG_BOOL || f->type == FLAG_COUNT) {
                    f->value.bool_val = true;
                    if (f->type == FLAG_COUNT) f->value.count_val++;
                    f->parsed = true;
                } else if (i + 1 < argc) {
                    parse_value(f, argv[++i]);
                }
            }
        } else {
            bool is_subcmd = false;
            for (int s = 0; s < cmd->subcmd_count; s++) {
                if (strcmp(cmd->subcommands[s]->name, arg) == 0) {
                    cmd = cmd->subcommands[s];
                    cmd->parsed = true;
                    cp->current = cmd;
                    is_subcmd = true;
                    break;
                }
            }
            if (!is_subcmd) {
                if (cp->arg_count < MAX_ARGS)
                    cp->args[cp->arg_count++] = arg;
            }
        }
    }

    if (cmd->handler) cmd->handler(cmd, cp->arg_count, cp->args);
    return 0;
}

const char* cli_get_string(const CLIParser *cp, const char *name, const char *def) {
    if (!cp || !name) return def;
    Flag *f = find_flag(cp->current, name);
    return (f && f->parsed) ? f->value.str_val : def;
}

int64_t cli_get_int(const CLIParser *cp, const char *name, int64_t def) {
    if (!cp || !name) return def;
    Flag *f = find_flag(cp->current, name);
    return (f && f->parsed) ? f->value.int_val : def;
}

double cli_get_float(const CLIParser *cp, const char *name, double def) {
    if (!cp || !name) return def;
    Flag *f = find_flag(cp->current, name);
    return (f && f->parsed) ? f->value.float_val : def;
}

bool cli_get_bool(const CLIParser *cp, const char *name, bool def) {
    if (!cp || !name) return def;
    Flag *f = find_flag(cp->current, name);
    return (f && f->parsed) ? f->value.bool_val : def;
}

int cli_get_count(const CLIParser *cp, const char *name) {
    if (!cp || !name) return 0;
    Flag *f = find_flag(cp->current, name);
    return (f && f->parsed) ? f->value.count_val : 0;
}

bool cli_flag_parsed(const CLIParser *cp, const char *name) {
    if (!cp || !name) return false;
    Flag *f = find_flag(cp->current, name);
    return f ? f->parsed : false;
}

void cli_generate_help(const CLIParser *cp, char *buf, int buf_sz) {
    if (!cp || !buf) return;
    int off = 0;
    const CLICommand *cmd = cp->current;
    off += snprintf(buf + off, buf_sz - off, "%s - %s\n\n", cmd->name, cmd->description);
    off += snprintf(buf + off, buf_sz - off, "%s", "Flags:\n");
    for (int i = 0; i < cmd->flag_count; i++) {
        const Flag *f = &cmd->flags[i];
        off += snprintf(buf + off, buf_sz - off, "  --%s", f->name);
        if (f->short_name) off += snprintf(buf + off, buf_sz - off, ", -%c", f->short_name);
        off += snprintf(buf + off, buf_sz - off, "  <%s>  %s\n",
                        flag_type_str(f->type), f->help);
    }
    if (cmd->subcmd_count > 0) {
        off += snprintf(buf + off, buf_sz - off, "%s", "\nCommands:\n");
        for (int i = 0; i < cmd->subcmd_count; i++)
            off += snprintf(buf + off, buf_sz - off, "  %-20s %s\n",
                            cmd->subcommands[i]->name,
                            cmd->subcommands[i]->description);
    }
    if (off < buf_sz) buf[off] = '\0';
}

void cli_print_help(const CLIParser *cp) {
    char buf[4096];
    cli_generate_help(cp, buf, sizeof(buf));
    printf("%s", buf);
}


int cli_flag_set_default_string(CLICommand *cmd, const char *name, const char *def) {
    if (!cmd || !name) return -1;
    Flag *f = find_flag(cmd, name);
    if (!f) return -1;
    snprintf(f->default_str, sizeof(f->default_str), "%s", def ? def : "");
    snprintf(f->value.str_val, sizeof(f->value.str_val), "%s", def ? def : "");
    return 0;
}

int cli_flag_set_default_int(CLICommand *cmd, const char *name, int64_t def) {
    if (!cmd || !name) return -1;
    Flag *f = find_flag(cmd, name);
    if (!f) return -1;
    f->default_int = def;
    f->value.int_val = def;
    return 0;
}

int cli_flag_set_default_float(CLICommand *cmd, const char *name, double def) {
    if (!cmd || !name) return -1;
    Flag *f = find_flag(cmd, name);
    if (!f) return -1;
    f->default_float = def;
    f->value.float_val = def;
    return 0;
}

int cli_flag_set_required(CLICommand *cmd, const char *name, bool required) {
    if (!cmd || !name) return -1;
    Flag *f = find_flag(cmd, name);
    if (!f) return -1;
    f->required = required;
    return 0;
}

int cli_get_positional_args(const CLIParser *cp, char ***args_out) {
    if (!cp || !args_out) return 0;
    *args_out = (char**)cp->args;
    return cp->arg_count;
}

bool cli_has_subcommand(const CLIParser *cp, const char *name) {
    if (!cp || !name) return false;
    for (int i = 0; i < cp->current->subcmd_count; i++)
        if (strcmp(cp->current->subcommands[i]->name, name) == 0)
            return true;
    return false;
}

int cli_validate_required(CLICommand *cmd, char *err_buf, int err_buf_sz) {
    if (!cmd) return 0;
    int missing = 0;
    for (int i = 0; i < cmd->flag_count; i++) {
        if (cmd->flags[i].required && !cmd->flags[i].parsed) {
            if (err_buf && missing == 0)
                snprintf(err_buf, err_buf_sz, "Missing required flag: --%s", cmd->flags[i].name);
            missing++;
        }
    }
    return missing;
}

void cli_reset(CLIParser *cp) {
    if (!cp) return;
    cp->arg_count = 0;
    cp->current = &cp->root;
    for (int i = 0; i < cp->root.flag_count; i++) {
        cp->root.flags[i].parsed = false;
        if (cp->root.flags[i].type == FLAG_COUNT) cp->root.flags[i].value.count_val = 0;
    }
}
