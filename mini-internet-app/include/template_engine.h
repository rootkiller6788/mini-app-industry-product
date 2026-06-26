#ifndef MINI_APP_TEMPLATE_ENGINE_H
#define MINI_APP_TEMPLATE_ENGINE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Simple server-side template engine with variable substitution,
 * conditionals, and loops. */

#define MINI_APP_TMPL_MAX_VARS   64
#define MINI_APP_TMPL_MAX_DEPTH  8
#define MINI_APP_TMPL_VAR_NAME   64
#define MINI_APP_TMPL_VAR_VALUE  256

typedef struct {
    char key[MINI_APP_TMPL_VAR_NAME];
    char value[MINI_APP_TMPL_VAR_VALUE];
} MiniAppTemplateVar;

typedef struct {
    MiniAppTemplateVar vars[MINI_APP_TMPL_MAX_VARS];
    int var_count;
    char *template_str;
    size_t template_len;
    char *output;
    size_t output_len;
    size_t output_capacity;
} MiniAppTemplateEngine;

void mini_app_template_init(MiniAppTemplateEngine *tmpl, const char *template_str);
void mini_app_template_set(MiniAppTemplateEngine *tmpl,
                            const char *key, const char *value);
void mini_app_template_set_int(MiniAppTemplateEngine *tmpl,
                                const char *key, int value);
char *mini_app_template_render(MiniAppTemplateEngine *tmpl);
void mini_app_template_free(MiniAppTemplateEngine *tmpl);

/* Template inheritance */
typedef struct {
    char name[64];
    char content[1024];
} MiniAppTemplateBlock;

#define MINI_APP_TMPL_MAX_BLOCKS 16
typedef struct {
    MiniAppTemplateBlock blocks[MINI_APP_TMPL_MAX_BLOCKS];
    int block_count;
    char parent_template[256];
} MiniAppTemplateInheritance;

void mini_app_template_inheritance_init(MiniAppTemplateInheritance *ti);
void mini_app_template_block_set(MiniAppTemplateInheritance *ti,
                                  const char *name, const char *content);
const char *mini_app_template_block_get(MiniAppTemplateInheritance *ti,
                                         const char *name);
bool mini_app_template_include(MiniAppTemplateEngine *tmpl,
                                const char *filename,
                                const char *(*load_file)(const char *, size_t *));
void mini_app_template_date_format(uint64_t timestamp, const char *format,
                                     char *buf, size_t buf_size);
void mini_app_template_format_number(uint64_t num, char *buf, size_t buf_size);
const char *mini_app_template_plural(int count, const char *singular,
                                      const char *plural);

/* HTML escaping utility */
void mini_app_html_escape(const char *src, char *dst, size_t dst_size);

#endif
