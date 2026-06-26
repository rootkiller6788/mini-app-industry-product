#include "code_review.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char* review_status_str(ReviewStatus s) {
    switch (s) {
    case REVIEW_PENDING:  return "pending";
    case REVIEW_APPROVED: return "approved";
    case REVIEW_CHANGES:  return "changes-requested";
    case REVIEW_REJECTED: return "rejected";
    case REVIEW_MERGED:   return "merged";
    default:              return "???";
    }
}

const char* check_status_str(CheckStatus s) {
    switch (s) {
    case CHECK_PASSED:  return "passed";
    case CHECK_FAILED:  return "failed";
    case CHECK_PENDING: return "pending";
    case CHECK_SKIPPED: return "skipped";
    default:            return "???";
    }
}

CodeReview* review_create(const char *title, const char *author, const char *branch) {
    CodeReview *cr = (CodeReview*)calloc(1, sizeof(CodeReview));
    if (!cr) return NULL;
    if (title) snprintf(cr->title, sizeof(cr->title), "%s", title);
    if (author) snprintf(cr->author, sizeof(cr->author), "%s", author);
    if (branch) snprintf(cr->branch, sizeof(cr->branch), "%s", branch);
    snprintf(cr->target_branch, sizeof(cr->target_branch), "main");
    cr->status = REVIEW_PENDING;
    cr->created_at = time(NULL);
    cr->updated_at = cr->created_at;
    cr->reviewer_count = 0;
    cr->file_count = 0;
    cr->comment_count = 0;
    cr->check_count = 0;
    cr->all_checks_passed = true;
    cr->unresolved_threads = 0;
    return cr;
}

void review_destroy(CodeReview *cr) { free(cr); }

int review_add_reviewer(CodeReview *cr, const char *reviewer) {
    if (!cr || !reviewer || cr->reviewer_count >= 8) return -1;
    snprintf(cr->reviewers[cr->reviewer_count], MAX_REVIEWER_NAME, "%s", reviewer);
    cr->reviewer_count++;
    return cr->reviewer_count - 1;
}

int review_add_file(CodeReview *cr, const char *filepath) {
    if (!cr || !filepath || cr->file_count >= MAX_REVIEW_FILES) return -1;
    snprintf(cr->changed_files[cr->file_count], 128, "%s", filepath);
    cr->file_count++;
    return cr->file_count - 1;
}

int review_add_comment(CodeReview *cr, const char *author, const char *body,
                        const char *file, int line) {
    if (!cr || !author || !body || cr->comment_count >= MAX_REVIEW_COMMENTS) return -1;
    ReviewComment *c = &cr->comments[cr->comment_count];
    memset(c, 0, sizeof(ReviewComment));
    snprintf(c->author, sizeof(c->author), "%s", author);
    snprintf(c->body, sizeof(c->body), "%s", body);
    if (file) snprintf(c->file, sizeof(c->file), "%s", file);
    c->line_start = line;
    c->line_end = line;
    c->created_at = time(NULL);
    c->resolved = false;
    cr->comment_count++;
    cr->unresolved_threads++;
    cr->updated_at = time(NULL);
    return cr->comment_count - 1;
}

int review_resolve_comment(CodeReview *cr, int comment_idx, const char *who) {
    if (!cr || comment_idx < 0 || comment_idx >= cr->comment_count) return -1;
    if (!cr->comments[comment_idx].resolved) {
        cr->comments[comment_idx].resolved = true;
        if (who) snprintf(cr->comments[comment_idx].resolved_by,
                          sizeof(cr->comments[comment_idx].resolved_by), "%s", who);
        cr->unresolved_threads--;
    }
    return 0;
}

int review_add_check(CodeReview *cr, const char *name, CheckStatus s,
                      double duration_s, const char *output) {
    if (!cr || !name || cr->check_count >= 16) return -1;
    CodeCheck *c = &cr->checks[cr->check_count];
    memset(c, 0, sizeof(CodeCheck));
    snprintf(c->name, sizeof(c->name), "%s", name);
    c->status = s;
    c->duration_seconds = duration_s;
    if (output) snprintf(c->output, sizeof(c->output), "%s", output);
    cr->check_count++;
    if (s != CHECK_PASSED) cr->all_checks_passed = false;
    return cr->check_count - 1;
}

int review_approve(CodeReview *cr) {
    if (!cr) return -1;
    if (cr->unresolved_threads > 0) return -2;
    cr->status = REVIEW_APPROVED;
    cr->updated_at = time(NULL);
    return 0;
}

int review_request_changes(CodeReview *cr) {
    if (!cr) return -1;
    cr->status = REVIEW_CHANGES;
    cr->updated_at = time(NULL);
    return 0;
}

int review_reject(CodeReview *cr) {
    if (!cr) return -1;
    cr->status = REVIEW_REJECTED;
    cr->updated_at = time(NULL);
    return 0;
}

int review_merge(CodeReview *cr) {
    if (!cr || cr->status != REVIEW_APPROVED) return -1;
    if (!cr->all_checks_passed) return -2;
    cr->status = REVIEW_MERGED;
    cr->updated_at = time(NULL);
    return 0;
}

bool review_all_checks_passed(const CodeReview *cr) {
    return cr ? cr->all_checks_passed : false;
}

int review_stats(const CodeReview *cr, int *total, int *resolved, int *unresolved) {
    if (!cr) return -1;
    if (total) *total = cr->comment_count;
    if (resolved) *resolved = cr->comment_count - cr->unresolved_threads;
    if (unresolved) *unresolved = cr->unresolved_threads;
    return 0;
}

void review_dump(const CodeReview *cr, char *buf, int buf_sz) {
    if (!cr || !buf) return;
    int off = 0;
    off += snprintf(buf + off, buf_sz - off, "%s", "=== Code Review ===\n");
    off += snprintf(buf + off, buf_sz - off, "Title: %s  Status: %s\n",
                    cr->title, review_status_str(cr->status));
    off += snprintf(buf + off, buf_sz - off, "Author: %s  Branch: %s -> %s\n",
                    cr->author, cr->branch, cr->target_branch);
    off += snprintf(buf + off, buf_sz - off, "Files: %d  Comments: %d  Unresolved: %d\n",
                    cr->file_count, cr->comment_count, cr->unresolved_threads);
    off += snprintf(buf + off, buf_sz - off, "Checks: %d  All passed: %s\n",
                    cr->check_count, cr->all_checks_passed ? "yes" : "no");
    if (off < buf_sz) buf[off] = '\0';
}


int review_add_multiple_files(CodeReview *cr, const char **files, int count) {
    if (!cr || !files || count <= 0) return -1;
    int added = 0;
    for (int i = 0; i < count; i++)
        if (review_add_file(cr, files[i]) >= 0) added++;
    return added;
}

int review_get_unresolved_comments(const CodeReview *cr, int *indices, int max_count) {
    if (!cr || !indices || max_count <= 0) return 0;
    int count = 0;
    for (int i = 0; i < cr->comment_count && count < max_count; i++)
        if (!cr->comments[i].resolved) indices[count++] = i;
    return count;
}

bool review_is_mergeable(const CodeReview *cr) {
    return cr && cr->status == REVIEW_APPROVED &&
           cr->all_checks_passed && cr->unresolved_threads == 0;
}

int review_get_check_summary(const CodeReview *cr, int *passed, int *failed, int *pending) {
    if (!cr) return -1;
    int p = 0, f = 0, pe = 0;
    for (int i = 0; i < cr->check_count; i++) {
        if (cr->checks[i].status == CHECK_PASSED) p++;
        else if (cr->checks[i].status == CHECK_FAILED) f++;
        else if (cr->checks[i].status == CHECK_PENDING) pe++;
    }
    if (passed) *passed = p;
    if (failed) *failed = f;
    if (pending) *pending = pe;
    return 0;
}

double review_time_to_merge_hours(const CodeReview *cr) {
    if (!cr || cr->status != REVIEW_MERGED) return 0.0;
    return difftime(cr->updated_at, cr->created_at) / 3600.0;
}
