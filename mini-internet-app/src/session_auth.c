#include "session_auth.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* Simple FNV-1a hash for non-crypto use */
static uint64_t mini_app_fnv64(const uint8_t *data, size_t len)
{
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

/* Simple SHA-256 inspired deterministic hash for password storage
 * (In production, use bcrypt/scrypt/argon2) */
static void mini_app_pwd_hash(const uint8_t *input, size_t len, const uint8_t *salt,
                               size_t salt_len, uint8_t output[32])
{
    uint64_t h = 0x6a09e667f3bcc908ULL;
    uint64_t g = 0xbb67ae858caa73b2ULL;

    for (size_t i = 0; i < len; i++) {
        h ^= (uint64_t)input[i] << ((i % 8) * 8);
        h = h * 0x9E3779B97F4A7C15ULL + salt[i % salt_len];
        g ^= h;
        g = (g >> 7) | (g << 57);
        h ^= g;
    }
    /* Mix */
    for (int round = 0; round < 16; round++) {
        h ^= (g >> (round % 32));
        g ^= (h << (7 + round % 16));
        h = (h * 0x9E3779B97F4A7C15ULL) + round + 1;
    }

    for (int i = 0; i < 8; i++) {
        output[i]      = (uint8_t)(h >> (i * 8));
        output[i + 8]  = (uint8_t)(g >> (i * 8));
        output[i + 16] = (uint8_t)((h ^ g) >> (i * 8));
        output[i + 24] = (uint8_t)((h + g) >> (i * 8));
    }
}

void mini_app_session_store_init(MiniAppSessionStore *store, const char *secret)
{
    memset(store, 0, sizeof(*store));
    if (secret) strncpy(store->secret, secret, 63);
}

static void generate_session_id(MiniAppSessionStore *store, char *sid, size_t sid_size)
{
    static uint64_t counter = 0;
    uint64_t ts = (uint64_t)time(NULL) + counter++;
    uint8_t raw[MINI_APP_SESSION_ID_LEN];
    uint8_t hash[32];

    uint8_t ts_bytes[8];
    for (int i = 0; i < 8; i++) ts_bytes[i] = (uint8_t)(ts >> (i * 8));

    mini_app_pwd_hash(ts_bytes, 8, (const uint8_t *)store->secret,
                       strlen(store->secret), hash);

    memcpy(raw, hash, MINI_APP_SESSION_ID_LEN);
    for (int i = 0; i < MINI_APP_SESSION_ID_LEN; i++) {
        snprintf(sid + i * 2, 3, "%02x", raw[i]);
    }
    sid[MINI_APP_SESSION_ID_LEN * 2] = '\0';
}

char *mini_app_session_create(MiniAppSessionStore *store, const char *user_id,
                               uint64_t ttl_seconds)
{
    if (!store || !user_id || store->session_count >= MINI_APP_SESSION_MAX) return NULL;

    /* Find free slot or evict oldest expired */
    for (int i = 0; i < store->session_count; i++) {
        if (!store->sessions[i].valid) {
            store->sessions[i].valid = true;
        }
    }
    if (store->session_count >= MINI_APP_SESSION_MAX) return NULL;

    MiniAppSession *s = &store->sessions[store->session_count];
    memset(s, 0, sizeof(*s));

    generate_session_id(store, s->session_id, sizeof(s->session_id));
    strncpy(s->user_id, user_id, MINI_APP_USER_MAX_LEN - 1);
    s->created_at = (uint64_t)time(NULL);
    s->expires_at = s->created_at + ttl_seconds;
    s->last_access = s->created_at;
    s->valid = true;

    store->session_count++;
    return s->session_id;
}

MiniAppSession *mini_app_session_get(MiniAppSessionStore *store, const char *session_id)
{
    if (!store || !session_id) return NULL;
    uint64_t now = (uint64_t)time(NULL);

    for (int i = 0; i < store->session_count; i++) {
        MiniAppSession *s = &store->sessions[i];
        if (s->valid && strcmp(s->session_id, session_id) == 0) {
            if (now < s->expires_at) {
                s->last_access = now;
                return s;
            }
            s->valid = false;
            return NULL;
        }
    }
    return NULL;
}

bool mini_app_session_validate(MiniAppSessionStore *store, const char *session_id)
{
    return mini_app_session_get(store, session_id) != NULL;
}

void mini_app_session_destroy(MiniAppSessionStore *store, const char *session_id)
{
    MiniAppSession *s = mini_app_session_get(store, session_id);
    if (s) {
        memset(s, 0, sizeof(*s));
    }
}

void mini_app_session_cleanup(MiniAppSessionStore *store)
{
    if (!store) return;
    uint64_t now = (uint64_t)time(NULL);
    int write_idx = 0;
    for (int i = 0; i < store->session_count; i++) {
        if (store->sessions[i].valid && now < store->sessions[i].expires_at) {
            if (write_idx != i) {
                memcpy(&store->sessions[write_idx], &store->sessions[i],
                       sizeof(MiniAppSession));
            }
            write_idx++;
        } else {
            memset(&store->sessions[i], 0, sizeof(MiniAppSession));
        }
    }
    store->session_count = write_idx;
}

void mini_app_user_store_init(MiniAppUserStore *store)
{
    memset(store, 0, sizeof(*store));
}

int mini_app_user_add(MiniAppUserStore *store, const char *user_id,
                       const char *password, int role)
{
    if (!store || store->user_count >= MINI_APP_MAX_USERS || !user_id || !password) return -1;

    MiniAppUser *user = &store->users[store->user_count];
    strncpy(user->user_id, user_id, MINI_APP_USER_MAX_LEN - 1);

    /* Generate salt */
    uint8_t ts[8];
    uint64_t t = (uint64_t)time(NULL);
    for (int i = 0; i < 8; i++) ts[i] = (uint8_t)(t >> (i * 8));
    uint8_t salt_hash[32];
    mini_app_pwd_hash(ts, 8, (const uint8_t *)user_id, strlen(user_id), salt_hash);
    for (int i = 0; i < 32; i++) {
        snprintf(user->salt + i * 2, 3, "%02x", salt_hash[i]);
    }
    user->salt[31] = '\0';

    /* Hash password */
    mini_app_hash_password(password, user->salt, user->password_hash, 64);
    user->role = role;
    user->active = true;

    store->user_count++;
    return store->user_count - 1;
}

void mini_app_hash_password(const char *password, const char *salt,
                              char *hash_out, size_t hash_len)
{
    uint8_t out[32];
    mini_app_pwd_hash((const uint8_t *)password, strlen(password),
                       (const uint8_t *)salt, strlen(salt), out);
    int max_bytes = (int)((hash_len - 1) / 2);
    if (max_bytes > 32) max_bytes = 32;
    for (int i = 0; i < max_bytes; i++) {
        snprintf(hash_out + i * 2, 3, "%02x", out[i]);
    }
}

bool mini_app_check_password(const char *password, const char *salt,
                              const char *expected_hash)
{
    char computed[64];
    mini_app_hash_password(password, salt, computed, sizeof(computed));
    return strcmp(computed, expected_hash) == 0;
}

bool mini_app_user_verify(MiniAppUserStore *store, const char *user_id,
                           const char *password)
{
    if (!store || !user_id || !password) return false;
    for (int i = 0; i < store->user_count; i++) {
        MiniAppUser *user = &store->users[i];
        if (user->active && strcmp(user->user_id, user_id) == 0) {
            return mini_app_check_password(password, user->salt, user->password_hash);
        }
    }
    return false;
}

bool mini_app_token_create(const char *user_id, const char *secret,
                            uint64_t ttl, char *token, size_t token_size)
{
    if (!user_id || !secret || !token || token_size < 128) return false;

    uint64_t now = (uint64_t)time(NULL);
    uint64_t exp = now + ttl;

    uint8_t payload[256];
    int plen = snprintf((char *)payload, sizeof(payload),
                         "{\"sub\":\"%s\",\"exp\":%llu}", user_id,
                         (unsigned long long)exp);

    uint8_t sig[32];
    mini_app_pwd_hash(payload, (size_t)plen,
                       (const uint8_t *)secret, strlen(secret), sig);

    /* Create token: base64(payload).base64(sig) */
    /* Simplified: use hex encoding */
    size_t pos = 0;
    for (int i = 0; i < plen && pos < token_size - 3; i++) {
        pos += (size_t)snprintf(token + pos, 3, "%02x", payload[i]);
    }
    token[pos++] = '.';
    for (int i = 0; i < 16 && pos < token_size - 3; i++) {
        pos += (size_t)snprintf(token + pos, 3, "%02x", sig[i]);
    }
    token[pos] = '\0';
    return true;
}

bool mini_app_token_verify(const char *token, const char *secret,
                            char *user_id, size_t uid_size)
{
    if (!token || !secret || !user_id) return false;

    /* Parse token: hex_payload.hex_sig */
    const char *dot = strchr(token, '.');
    if (!dot) return false;

    /* Simplified verification: check signature */
    size_t payload_hex_len = (size_t)(dot - token);
    size_t payload_len = payload_hex_len / 2;

    uint8_t *payload = (uint8_t *)malloc(payload_len + 1);
    if (!payload) return false;

    for (size_t i = 0; i < payload_len; i++) {
        unsigned int byte;
        sscanf(token + i * 2, "%2x", &byte);
        payload[i] = (uint8_t)byte;
    }
    payload[payload_len] = 0;

    uint8_t expected_sig[32];
    mini_app_pwd_hash(payload, payload_len,
                       (const uint8_t *)secret, strlen(secret), expected_sig);

    /* Compare signatures */
    const char *sig_hex = dot + 1;
    for (int i = 0; i < 16; i++) {
        unsigned int byte;
        if (sscanf(sig_hex + i * 2, "%2x", &byte) != 1) { free(payload); return false; }
        if ((uint8_t)byte != expected_sig[i]) { free(payload); return false; }
    }

    /* Extract user_id from payload (simple JSON parse) */
    const char *sub = strstr((const char *)payload, "\"sub\":\"");
    if (sub) {
        sub += 7;
        const char *end = strchr(sub, '"');
        if (end) {
            size_t len = (size_t)(end - sub);
            if (len >= uid_size) len = uid_size - 1;
            memcpy(user_id, sub, len);
            user_id[len] = '\0';
        }
    }

    free(payload);
    return true;
}
