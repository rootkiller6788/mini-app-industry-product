#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "web_server.h"
#include "json_api.h"
#include "session_auth.h"
#include "rate_limiter.h"

/* Demo: REST API server showcasing routing, auth, JSON, and rate limiting */

static MiniAppUserStore users;
static MiniAppSessionStore sessions;
static MiniAppRateLimiter rate_limit;

static void handle_login(const MiniAppHttpRequest *req,
                          MiniAppHttpResponse *res, void *ctx)
{
    (void)ctx;
    if (!mini_app_rate_limiter_allow(&rate_limit, req->remote_addr)) {
        res->status = MINI_APP_HTTP_TOO_MANY_REQUESTS;
        mini_app_response_set_body(res, (const uint8_t *)"{\"error\":\"Rate limited\"}", 23);
        return;
    }
    /* Parse credentials from body */
    MiniAppJsonNode *body = mini_app_json_parse((const char *)req->body);
    if (!body) { res->status = MINI_APP_HTTP_BAD_REQUEST; return; }

    MiniAppJsonNode *uid = mini_app_json_object_get(body, "username");
    MiniAppJsonNode *pwd = mini_app_json_object_get(body, "password");
    if (!uid || !pwd || uid->type != MINI_APP_JSON_STRING || pwd->type != MINI_APP_JSON_STRING) {
        res->status = MINI_APP_HTTP_BAD_REQUEST;
        mini_app_json_free(body);
        return;
    }

    if (mini_app_user_verify(&users, uid->value.str_val, pwd->value.str_val)) {
        char *sid = mini_app_session_create(&sessions, uid->value.str_val, 3600);

        MiniAppJsonNode *resp = mini_app_json_create_object();
        mini_app_json_object_set(resp, "token", mini_app_json_create_string(sid));
        mini_app_json_object_set(resp, "status", mini_app_json_create_string("ok"));

        char buf[1024];
        mini_app_json_serialize(resp, buf, sizeof(buf));
        mini_app_response_set_body(res, (const uint8_t *)buf, strlen(buf));
        mini_app_response_add_header(res, "Content-Type", "application/json");
        mini_app_json_free(resp);
    } else {
        res->status = MINI_APP_HTTP_UNAUTHORIZED;
        mini_app_response_set_body(res,
            (const uint8_t *)"{\"error\":\"Invalid credentials\"}", 29);
    }
    mini_app_json_free(body);
}

static void handle_profile(const MiniAppHttpRequest *req,
                            MiniAppHttpResponse *res, void *ctx)
{
    (void)ctx;
    /* Check auth token */
    const char *auth = NULL;
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].key, "Authorization") == 0) {
            auth = req->headers[i].value;
            break;
        }
    }
    if (!auth || strncmp(auth, "Bearer ", 7) != 0) {
        res->status = MINI_APP_HTTP_UNAUTHORIZED;
        return;
    }

    MiniAppSession *s = mini_app_session_get(&sessions, auth + 7);
    if (!s) {
        res->status = MINI_APP_HTTP_UNAUTHORIZED;
        return;
    }

    MiniAppJsonNode *resp = mini_app_json_create_object();
    mini_app_json_object_set(resp, "user_id", mini_app_json_create_string(s->user_id));
    mini_app_json_object_set(resp, "created_at", mini_app_json_create_number((double)s->created_at));

    char buf[512];
    mini_app_json_serialize(resp, buf, sizeof(buf));
    mini_app_response_set_body(res, (const uint8_t *)buf, strlen(buf));
    mini_app_response_add_header(res, "Content-Type", "application/json");
    mini_app_json_free(resp);
}

int main(void)
{
    printf("=== Mini REST API Server Demo ===\n\n");

    /* Initialize backend stores */
    mini_app_user_store_init(&users);
    mini_app_user_add(&users, "alice", "password123", 0);
    mini_app_user_add(&users, "admin", "adminpass", 1);
    printf("Users initialized: alice (user), admin (admin)\n");

    mini_app_session_store_init(&sessions, "demo-secret-key");
    mini_app_rate_limiter_init(&rate_limit, 100.0, 10.0);
    printf("Rate limiter: 100 burst, 10/sec refill\n\n");

    /* Setup web server */
    MiniAppWebServer srv;
    mini_app_server_init(&srv, "MiniREST/1.0");
    mini_app_server_add_route(&srv, MINI_APP_HTTP_POST, "/api/login", handle_login, NULL);
    mini_app_server_add_route(&srv, MINI_APP_HTTP_GET, "/api/profile", handle_profile, NULL);
    printf("Routes: POST /api/login, GET /api/profile\n");

    /* Simulate login request */
    printf("\n--- Login Request ---\n");
    const char *login_body = "{\"username\":\"alice\",\"password\":\"password123\"}";
    MiniAppHttpRequest login_req;
    mini_app_request_init(&login_req);
    login_req.method = MINI_APP_HTTP_POST;
    strcpy(login_req.uri, "/api/login");
    strcpy(login_req.remote_addr, "192.168.1.100");
    login_req.body = (uint8_t *)strdup(login_body);
    login_req.body_len = strlen(login_body);

    MiniAppHttpResponse login_res;
    mini_app_response_init(&login_res);
    mini_app_server_dispatch(&srv, &login_req, &login_res);

    printf("Status: %s\n", mini_app_http_status_str(login_res.status));
    printf("Response: %s\n", login_res.body ? (char *)login_res.body : "(empty)");

    /* Extract token */
    char token[256] = {0};
    if (login_res.body) {
        MiniAppJsonNode *lr = mini_app_json_parse((const char *)login_res.body);
        MiniAppJsonNode *t = mini_app_json_object_get(lr, "token");
        if (t && t->value.str_val) strcpy(token, t->value.str_val);
        mini_app_json_free(lr);
    }

    /* Simulate profile request */
    printf("\n--- Profile Request ---\n");
    MiniAppHttpRequest prof_req;
    mini_app_request_init(&prof_req);
    prof_req.method = MINI_APP_HTTP_GET;
    strcpy(prof_req.uri, "/api/profile");
    mini_app_request_parse(&prof_req, NULL, 0); /* Reset */
    prof_req.method = MINI_APP_HTTP_GET;
    strcpy(prof_req.uri, "/api/profile");
    char auth_hdr[512];
    snprintf(auth_hdr, sizeof(auth_hdr), "Bearer %s", token);
    strcpy(prof_req.headers[0].key, "Authorization");
    strcpy(prof_req.headers[0].value, auth_hdr);
    prof_req.header_count = 1;

    MiniAppHttpResponse prof_res;
    mini_app_response_init(&prof_res);
    mini_app_server_dispatch(&srv, &prof_req, &prof_res);

    printf("Status: %s\n", mini_app_http_status_str(prof_res.status));
    printf("Response: %s\n", prof_res.body ? (char *)prof_res.body : "(empty)");

    /* Cleanup */
    mini_app_request_free(&login_req);
    free(login_res.body);
    free(prof_res.body);

    printf("\nREST API demo completed.\n");
    return 0;
}
