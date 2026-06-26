#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_ARGS        128
#define MAX_SUBCMDS      16
#define MAX_FLAG_NAME    64
#define MAX_FLAG_HELP   256
#define MAX_CMD_DESC    256

typedef enum {
    FLAG_BOOL   = 0,
    FLAG_STRING = 1,
    FLAG_INT    = 2,
    FLAG_FLOAT  = 3,
    FLAG_COUNT  = 4
} FlagType;

typedef struct {
    char    name[MAX_FLAG_NAME];
    char    short_name;
    FlagType type;
    char    default_str[MAX_FLAG_HELP];
    int64_t default_int;
    double  default_float;
    bool    default_bool;
    char    help[MAX_FLAG_HELP];
    bool    required;
    bool    parsed;
    union {
        char str_val[MAX_FLAG_HELP];
        int64_t int_val;
        double float_val;
        bool bool_val;
        int count_val;
    } value;
} Flag;

typedef struct CLICommand CLICommand;
struct CLICommand {
    char   name[64];
    char   description[MAX_CMD_DESC];
    Flag   flags[MAX_ARGS];
    int    flag_count;
    int  (*handler)(CLICommand *cmd, int argc, char **argv);
    CLICommand *subcommands[MAX_SUBCMDS];
    int    subcmd_count;
    bool   parsed;
};

typedef struct {
    CLICommand root;
    CLICommand *current;
    int    arg_count;
    char   *args[MAX_ARGS];
    char   help_text[4096];
} CLIParser;

CLIParser*   cli_create(const char *name, const char *desc);
void         cli_destroy(CLIParser *cp);
int          cli_add_flag(CLICommand *cmd, const char *name, char short_name,
                          FlagType t, const char *help);
int          cli_add_subcommand(CLICommand *parent, const char *name,
                                 const char *desc, int (*handler)(CLICommand*, int, char**));
int          cli_parse(CLIParser *cp, int argc, char **argv);
const char*  cli_get_string(const CLIParser *cp, const char *name, const char *def);
int64_t      cli_get_int(const CLIParser *cp, const char *name, int64_t def);
double       cli_get_float(const CLIParser *cp, const char *name, double def);
bool         cli_get_bool(const CLIParser *cp, const char *name, bool def);
int          cli_get_count(const CLIParser *cp, const char *name);
bool         cli_flag_parsed(const CLIParser *cp, const char *name);
void         cli_generate_help(const CLIParser *cp, char *buf, int buf_sz);
void         cli_print_help(const CLIParser *cp);
const char*  flag_type_str(FlagType t);

#endif
