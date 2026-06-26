#ifndef CODE_REVIEW_H
#define CODE_REVIEW_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_REVIEW_COMMENTS  256
#define MAX_REVIEW_FILES     64
#define MAX_REVIEWER_NAME    48
#define MAX_REVIEW_TITLE    128
#define MAX_COMMENT_BODY    512
#define MAX_CHECK_NAME       64

typedef enum {
    REVIEW_PENDING  = 0,
    REVIEW_APPROVED = 1,
    REVIEW_CHANGES  = 2,
    REVIEW_REJECTED = 3,
    REVIEW_MERGED   = 4
} ReviewStatus;

typedef enum {
    CHECK_PASSED  = 0,
    CHECK_FAILED  = 1,
    CHECK_PENDING = 2,
    CHECK_SKIPPED = 3
} CheckStatus;

typedef struct {
    char   author[MAX_REVIEWER_NAME];
    char   body[MAX_COMMENT_BODY];
    char   file[128];
    int    line_start;
    int    line_end;
    time_t created_at;
    bool   resolved;
    char   resolved_by[MAX_REVIEWER_NAME];
} ReviewComment;

typedef struct {
    char   name[MAX_CHECK_NAME];
    CheckStatus status;
    double duration_seconds;
    char   output[256];
} CodeCheck;

typedef struct {
    char   title[MAX_REVIEW_TITLE];
    char   author[MAX_REVIEWER_NAME];
    char   reviewers[MAX_REVIEWER_NAME][8];
    int    reviewer_count;
    char   changed_files[MAX_REVIEW_FILES][128];
    int    file_count;
    ReviewComment comments[MAX_REVIEW_COMMENTS];
    int    comment_count;
    CodeCheck checks[16];
    int    check_count;
    ReviewStatus status;
    bool   all_checks_passed;
    int    unresolved_threads;
    time_t created_at;
    time_t updated_at;
    char   branch[128];
    char   target_branch[64];
} CodeReview;

CodeReview*   review_create(const char *title, const char *author, const char *branch);
void          review_destroy(CodeReview *cr);
int           review_add_reviewer(CodeReview *cr, const char *reviewer);
int           review_add_file(CodeReview *cr, const char *filepath);
int           review_add_comment(CodeReview *cr, const char *author, const char *body,
                                  const char *file, int line);
int           review_resolve_comment(CodeReview *cr, int comment_idx, const char *who);
int           review_add_check(CodeReview *cr, const char *name, CheckStatus s,
                                double duration_s, const char *output);
int           review_approve(CodeReview *cr);
int           review_request_changes(CodeReview *cr);
int           review_reject(CodeReview *cr);
int           review_merge(CodeReview *cr);
bool          review_all_checks_passed(const CodeReview *cr);
int           review_stats(const CodeReview *cr, int *total, int *resolved, int *unresolved);
int           review_add_multiple_files(CodeReview *cr, const char **files, int count);
int           review_get_unresolved_comments(const CodeReview *cr, int *indices, int max_count);
bool          review_is_mergeable(const CodeReview *cr);
int           review_get_check_summary(const CodeReview *cr, int *passed, int *failed, int *pending);
double        review_time_to_merge_hours(const CodeReview *cr);
const char*   review_status_str(ReviewStatus s);
const char*   check_status_str(CheckStatus s);
void          review_dump(const CodeReview *cr, char *buf, int buf_sz);

#endif
