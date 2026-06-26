#include "template_engine.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

/* ============================================================================
 * L5: Server-Side Template Engine
 *
 * A simple template engine supporting:
 * - Variable substitution: {{ variable_name }}
 * - Conditional blocks: {% if var %} ... {% endif %}
 * - Loop blocks: {% for item in list %} ... {% endfor %}
 * - HTML escaping for XSS prevention
 *
 * Rendering pipeline:
 * 1. Parse template into token stream
 * 2. Evaluate tokens with variable context
 * 3. Concatenate output
 *
 * L4: XSS Prevention
 * All user-supplied data must be HTML-escaped before rendering.
 * Context-sensitive escaping (HTML body vs attribute vs JS) not
 * implemented - production systems should use a full template engine
 * with context-aware auto-escaping (e.g., Jinja2, Go templates).
 * ========================================================================== */

void mini_app_template_init(MiniAppTemplateEngine *tmpl, const char *template_str)
{
    memset(tmpl, 0, sizeof(*tmpl));
    tmpl->template_str = strdup(template_str);
    tmpl->template_len = strlen(template_str);
    tmpl->output_capacity = 4096;
    tmpl->output = (char *)malloc(tmpl->output_capacity);
    if (tmpl->output) tmpl->output[0] = '\0';
}

void mini_app_template_set(MiniAppTemplateEngine *tmpl,
                            const char *key, const char *value)
{
    if (!tmpl || !key || !value) return;
    if (tmpl->var_count >= MINI_APP_TMPL_MAX_VARS) return;
    strncpy(tmpl->vars[tmpl->var_count].key, key, MINI_APP_TMPL_VAR_NAME - 1);
    strncpy(tmpl->vars[tmpl->var_count].value, value, MINI_APP_TMPL_VAR_VALUE - 1);
    tmpl->var_count++;
}

void mini_app_template_set_int(MiniAppTemplateEngine *tmpl,
                                const char *key, int value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    mini_app_template_set(tmpl, key, buf);
}

static const char *lookup_var(MiniAppTemplateEngine *tmpl, const char *name)
{
    for (int i = 0; i < tmpl->var_count; i++) {
        if (strcmp(tmpl->vars[i].key, name) == 0) {
            return tmpl->vars[i].value;
        }
    }
    return "";
}

static void append_output(MiniAppTemplateEngine *tmpl, const char *s, size_t len)
{
    if (!s || len == 0) return;
    size_t need = tmpl->output_len + len + 1;
    if (need > tmpl->output_capacity) {
        tmpl->output_capacity = need * 2;
        char *new_out = (char *)realloc(tmpl->output, tmpl->output_capacity);
        if (!new_out) return;
        tmpl->output = new_out;
    }
    memcpy(tmpl->output + tmpl->output_len, s, len);
    tmpl->output_len += len;
    tmpl->output[tmpl->output_len] = '\0';
}

char *mini_app_template_render(MiniAppTemplateEngine *tmpl)
{
    if (!tmpl || !tmpl->template_str || !tmpl->output) return NULL;

    tmpl->output_len = 0;
    tmpl->output[0] = '\0';

    const char *p = tmpl->template_str;
    const char *end = p + tmpl->template_len;

    int if_depth = 0;
    bool skip_stack[MINI_APP_TMPL_MAX_DEPTH];
    memset(skip_stack, 0, sizeof(skip_stack));

    while (p < end) {
        /* Look for template tags */
        const char *tag_start = strstr(p, "{{");
        const char *block_start = strstr(p, "{%");

        /* Determine which comes first */
        const char *next_tag = NULL;
        int tag_type = 0; /* 0=none, 1=var, 2=block */

        if (tag_start && (!block_start || tag_start < block_start)) {
            next_tag = tag_start;
            tag_type = 1;
        } else if (block_start) {
            next_tag = block_start;
            tag_type = 2;
        }

        if (!next_tag) {
            /* No more tags, output remaining text */
            if (!skip_stack[if_depth]) {
                append_output(tmpl, p, (size_t)(end - p));
            }
            break;
        }

        /* Output text before tag */
        if (!skip_stack[if_depth]) {
            append_output(tmpl, p, (size_t)(next_tag - p));
        }

        if (tag_type == 1) {
            /* Variable: {{ name }} */
            const char *var_end = strstr(next_tag + 2, "}}");
            if (!var_end) {
                append_output(tmpl, "{{", 2);
                p = next_tag + 2;
                continue;
            }
            /* Extract variable name */
            const char *var_name = next_tag + 2;
            size_t name_len = (size_t)(var_end - var_name);
            /* Trim whitespace */
            while (name_len > 0 && isspace((unsigned char)*var_name)) {
                var_name++; name_len--;
            }
            while (name_len > 0 && isspace((unsigned char)var_name[name_len - 1])) {
                name_len--;
            }

            char vname[MINI_APP_TMPL_VAR_NAME];
            size_t copy_len = name_len < sizeof(vname) - 1 ? name_len : sizeof(vname) - 1;
            memcpy(vname, var_name, copy_len);
            vname[copy_len] = '\0';

            const char *value = lookup_var(tmpl, vname);
            append_output(tmpl, value, strlen(value));

            p = var_end + 2;
        } else {
            /* Block: {% keyword %} */
            const char *block_end = strstr(next_tag + 2, "%}");
            if (!block_end) {
                append_output(tmpl, "{%", 2);
                p = next_tag + 2;
                continue;
            }

            const char *cmd_start = next_tag + 2;
            size_t cmd_len = (size_t)(block_end - cmd_start);
            while (cmd_len > 0 && isspace((unsigned char)*cmd_start)) {
                cmd_start++; cmd_len--;
            }
            while (cmd_len > 0 && isspace((unsigned char)cmd_start[cmd_len - 1])) {
                cmd_len--;
            }

            char cmd[64];
            size_t clen = cmd_len < sizeof(cmd) - 1 ? cmd_len : sizeof(cmd) - 1;
            memcpy(cmd, cmd_start, clen);
            cmd[clen] = '\0';

            if (strncmp(cmd, "if ", 3) == 0) {
                /* {% if condition %} */
                const char *cond = cmd + 3;
                while (*cond == ' ') cond++;
                const char *val = lookup_var(tmpl, cond);
                bool truthy = (strlen(val) > 0 && strcmp(val, "0") != 0 &&
                               strcasecmp(val, "false") != 0);
                if_depth++;
                if (if_depth < MINI_APP_TMPL_MAX_DEPTH) {
                    skip_stack[if_depth] = skip_stack[if_depth - 1] || !truthy;
                }
            } else if (strcmp(cmd, "endif") == 0) {
                if (if_depth > 0) if_depth--;
            } else if (strncmp(cmd, "for ", 4) == 0) {
                /* {% for var in list %} - simplified: just include once */
                /* Full implementation would iterate */
                const char *rest = cmd + 4;
                /* Parse: "item in list_name" */
                const char *in = strstr(rest, " in ");
                if (in) {
                    size_t item_len = (size_t)(in - rest);
                    const char *list_name = in + 4;
                    while (*list_name == ' ') list_name++;

                    char list_key[64];
                    strncpy(list_key, list_name, 63);
                    list_key[63] = '\0';
                    const char *list_val = lookup_var(tmpl, list_key);

                    /* Simple comma-separated list iteration */
                    char list_copy[512];
                    strncpy(list_copy, list_val, 511);
                    list_copy[511] = '\0';

                    char *saveptr = NULL;
                    char *item = strtok_r(list_copy, ",", &saveptr);
                    while (item) {
                        while (*item == ' ') item++;
                        /* Set loop variable */
                        char item_var[64];
                        memcpy(item_var, rest, item_len);
                        item_var[item_len] = '\0';
                        /* Find var end */
                        char *sp = strchr(item_var, ' ');
                        if (sp) *sp = '\0';

                        mini_app_template_set(tmpl, item_var, item);

                        /* Process body until {% endfor %} */
                        const char *body_start = block_end + 2;
                        const char *ef = strstr(body_start, "{% endfor %}");
                        if (ef && !skip_stack[if_depth]) {
                            /* Simple one-var subs in body */
                            const char *bp = body_start;
                            while (bp < ef) {
                                const char *vt = strstr(bp, "{{");
                                if (vt && vt < ef) {
                                    append_output(tmpl, bp, (size_t)(vt - bp));
                                    const char *ve = strstr(vt + 2, "}}");
                                    if (ve && ve < ef) {
                                        char vn[64];
                                        size_t vnl = (size_t)(ve - vt - 2);
                                        if (vnl > 63) vnl = 63;
                                        memcpy(vn, vt + 2, vnl);
                                        vn[vnl] = '\0';
                                        append_output(tmpl, lookup_var(tmpl, vn),
                                                      strlen(lookup_var(tmpl, vn)));
                                        bp = ve + 2;
                                    } else {
                                        append_output(tmpl, "{{", 2);
                                        bp = vt + 2;
                                    }
                                } else {
                                    append_output(tmpl, bp, (size_t)(ef - bp));
                                    break;
                                }
                            }
                            p = ef + 13; /* strlen("{% endfor %}") */
                        } else {
                            p = block_end + 2;
                        }
                        item = strtok_r(NULL, ",", &saveptr);
                    }
                } else {
                    p = block_end + 2;
                }
            } else if (strcmp(cmd, "endfor") == 0) {
                p = block_end + 2;
            } else {
                /* Unknown block tag, skip */
                p = block_end + 2;
            }
        }
    }

    return tmpl->output;
}

void mini_app_template_free(MiniAppTemplateEngine *tmpl)
{
    if (!tmpl) return;
    free(tmpl->template_str);
    free(tmpl->output);
    memset(tmpl, 0, sizeof(*tmpl));
}

/* ============================================================================
 * Additional Template Features: Includes, Macros, Filters
 * ========================================================================== */

/* Template include: {% include "filename" %} */
bool mini_app_template_include(MiniAppTemplateEngine *tmpl,
                                const char *filename,
                                const char *(*load_file)(const char *, size_t *))
{
    if (!tmpl || !filename || !load_file) return false;
    size_t inc_len = 0;
    const char *inc_content = load_file(filename, &inc_len);
    if (!inc_content) return false;

    /* Append included content to output */
    append_output(tmpl, inc_content, inc_len);
    return true;
}

/* Template inheritance: {% extends "base.html" %} ... {% block name %} ... {% endblock %} */

void mini_app_template_inheritance_init(MiniAppTemplateInheritance *ti)
{
    memset(ti, 0, sizeof(*ti));
}

void mini_app_template_block_set(MiniAppTemplateInheritance *ti,
                                  const char *name, const char *content)
{
    if (ti->block_count >= MINI_APP_TMPL_MAX_BLOCKS) return;
    strncpy(ti->blocks[ti->block_count].name, name, MINI_APP_TMPL_VAR_NAME - 1);
    strncpy(ti->blocks[ti->block_count].content, content,
            sizeof(ti->blocks[0].content) - 1);
    ti->block_count++;
}

const char *mini_app_template_block_get(MiniAppTemplateInheritance *ti,
                                         const char *name)
{
    for (int i = 0; i < ti->block_count; i++) {
        if (strcmp(ti->blocks[i].name, name) == 0) {
            return ti->blocks[i].content;
        }
    }
    return "";
}

/* Template date formatting helper */
void mini_app_template_date_format(uint64_t timestamp, const char *format,
                                     char *buf, size_t buf_size)
{
    time_t t = (time_t)timestamp;
    struct tm *tm_info = localtime(&t);
    if (tm_info && format && buf) {
        strftime(buf, buf_size, format, tm_info);
    } else if (buf) {
        snprintf(buf, buf_size, "%llu", (unsigned long long)timestamp);
    }
}

/* Number formatting with commas */
void mini_app_template_format_number(uint64_t num, char *buf, size_t buf_size)
{
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)num);
    size_t len = strlen(tmp);
    size_t commas = (len - 1) / 3;
    size_t out_len = len + commas;

    if (out_len >= buf_size) {
        strncpy(buf, tmp, buf_size - 1);
        return;
    }

    int di = (int)len - 1;
    int oi = (int)out_len;
    buf[oi--] = '\0';
    int cnt = 0;
    while (di >= 0) {
        if (cnt == 3) { buf[oi--] = ','; cnt = 0; }
        buf[oi--] = tmp[di--];
        cnt++;
    }
}

/* Pluralization helper */
const char *mini_app_template_plural(int count, const char *singular,
                                      const char *plural)
{
    return (count == 1) ? singular : plural;
}

void mini_app_html_escape(const char *src, char *dst, size_t dst_size)
{
    if (!src || !dst || dst_size == 0) return;
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 1; i++) {
        switch (src[i]) {
        case '&':  if (j + 5 < dst_size) { memcpy(dst + j, "&amp;", 5); j += 5; } break;
        case '<':  if (j + 4 < dst_size) { memcpy(dst + j, "&lt;", 4); j += 4; } break;
        case '>':  if (j + 4 < dst_size) { memcpy(dst + j, "&gt;", 4); j += 4; } break;
        case '"':  if (j + 6 < dst_size) { memcpy(dst + j, "&quot;", 6); j += 6; } break;
        case '\'': if (j + 5 < dst_size) { memcpy(dst + j, "&#39;", 5); j += 5; } break;
        default:   dst[j++] = src[i]; break;
        }
    }
    dst[j] = '\0';
}
