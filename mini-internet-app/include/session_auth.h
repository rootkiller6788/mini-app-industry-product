#ifndef MINI_APP_SESSION_AUTH_H
#define MINI_APP_SESSION_AUTH_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define MINI_APP_SESSION_ID_LEN   32
#define MINI_APP_SESSION_MAX      1024
#define MINI_APP_SESSION_DATA_MAX 512
#define MINI_APP_USER_MAX_LEN     64

typedef struct {
    char     session_id[MINI_APP_SESSION_ID_LEN * 2 + 1];
    char     user_id[MINI_APP_USER_MAX_LEN];
    uint8_t  data[MINI_APP_SESSION_DATA_MAX];
    size_t   data_len;
    uint64_t created_at;
    uint64_t expires_at;
    uint64_t last_access;
    bool     valid;
} MiniAppSession;

typedef struct {
    MiniAppSession sessions[MINI_APP_SESSION_MAX];
    int    session_count;
    char   secret[64];
} MiniAppSessionStore;

typedef enum {
    MINI_APP_AUTH_BASIC  = 0,
    MINI_APP_AUTH_BEARER = 1,
    MINI_APP_AUTH_SESSION = 2,
    MINI_APP_AUTH_NONE   = 3,
} MiniAppAuthType;

typedef struct {
    char     user_id[MINI_APP_USER_MAX_LEN];
    char     password_hash[64];
    char     salt[32];
    int      role;       /* 0=user, 1=admin */
    bool     active;
} MiniAppUser;

#define MINI_APP_MAX_USERS 256
typedef struct {
    MiniAppUser users[MINI_APP_MAX_USERS];
    int user_count;
} MiniAppUserStore;

void mini_app_session_store_init(MiniAppSessionStore *store, const char *secret);
char *mini_app_session_create(MiniAppSessionStore *store, const char *user_id,
                               uint64_t ttl_seconds);
MiniAppSession *mini_app_session_get(MiniAppSessionStore *store, const char *session_id);
bool mini_app_session_validate(MiniAppSessionStore *store, const char *session_id);
void mini_app_session_destroy(MiniAppSessionStore *store, const char *session_id);
void mini_app_session_cleanup(MiniAppSessionStore *store);

void mini_app_user_store_init(MiniAppUserStore *store);
int  mini_app_user_add(MiniAppUserStore *store, const char *user_id,
                        const char *password, int role);
bool mini_app_user_verify(MiniAppUserStore *store, const char *user_id,
                           const char *password);
void mini_app_hash_password(const char *password, const char *salt,
                             char *hash_out, size_t hash_len);
bool mini_app_check_password(const char *password, const char *salt,
                              const char *expected_hash);

/* JWT-like token (simplified) */
bool mini_app_token_create(const char *user_id, const char *secret,
                            uint64_t ttl, char *token, size_t token_size);
bool mini_app_token_verify(const char *token, const char *secret,
                            char *user_id, size_t uid_size);

#endif
