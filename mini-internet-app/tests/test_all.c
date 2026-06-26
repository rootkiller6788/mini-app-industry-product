#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "web_server.h"
#include "url_router.h"
#include "session_auth.h"
#include "json_api.h"
#include "rate_limiter.h"
#include "template_engine.h"
#include "content_cache.h"

static int passed = 0, failed = 0;

#define TEST(n) do { printf("  TEST: %s ... ", n); } while(0)
#define PASS() do { passed++; printf("PASS\n"); } while(0)
#define FAIL(m) do { failed++; printf("FAIL: %s\n", m); } while(0)
#define CHECK(c) do { if(!(c)){FAIL(#c);return;} } while(0)

static void t01_request_parse(void) {
    TEST("HTTP request parsing");
    const char *raw = "GET /api/v1/users HTTP/1.1\r\nHost: example.com\r\nAccept: application/json\r\n\r\n";
    MiniAppHttpRequest req;
    mini_app_request_init(&req);
    int rc = mini_app_request_parse(&req, (const uint8_t *)raw, strlen(raw));
    CHECK(rc == 0);
    CHECK(req.method == MINI_APP_HTTP_GET);
    CHECK(strcmp(req.uri, "/api/v1/users") == 0);
    CHECK(req.header_count == 2);
    mini_app_request_free(&req);
    PASS();
}

static void t02_response_serialize(void) {
    TEST("HTTP response serialization");
    MiniAppHttpResponse res;
    mini_app_response_init(&res);
    res.status = MINI_APP_HTTP_OK;
    mini_app_response_add_header(&res, "Content-Type", "application/json");
    mini_app_response_set_body(&res, (const uint8_t *)"{\"status\":\"ok\"}", 15);

    uint8_t buf[4096];
    size_t len = mini_app_response_serialize(&res, buf, sizeof(buf));
    CHECK(len > 0);
    CHECK(strstr((const char *)buf, "200 OK") != NULL);
    CHECK(strstr((const char *)buf, "Content-Type: application/json") != NULL);
    CHECK(strstr((const char *)buf, "{\"status\":\"ok\"}") != NULL);
    free(res.body);
    PASS();
}

static void handle_hello(const MiniAppHttpRequest *req, MiniAppHttpResponse *res, void *ctx) {
    (void)req; (void)ctx;
    res->status = MINI_APP_HTTP_OK;
    mini_app_response_add_header(res, "Content-Type", "text/plain");
    mini_app_response_set_body(res, (const uint8_t *)"Hello, World!", 13);
}

static void t03_web_server_routing(void) {
    TEST("Web server route dispatch");
    MiniAppWebServer srv;
    mini_app_server_init(&srv, "TestServer");
    mini_app_server_add_route(&srv, MINI_APP_HTTP_GET, "/hello", handle_hello, NULL);

    MiniAppHttpRequest req;
    mini_app_request_init(&req);
    req.method = MINI_APP_HTTP_GET;
    strcpy(req.uri, "/hello");

    MiniAppHttpResponse res;
    mini_app_response_init(&res);
    int rc = mini_app_server_dispatch(&srv, &req, &res);
    CHECK(rc == 0);
    CHECK(res.status == MINI_APP_HTTP_OK);
    CHECK(strncmp((const char *)res.body, "Hello, World!", 13) == 0);
    mini_app_request_free(&req);
    free(res.body);
    PASS();
}

static void t04_404_handler(void) {
    TEST("Web server 404 handler");
    MiniAppWebServer srv;
    mini_app_server_init(&srv, "TestServer");

    MiniAppHttpRequest req;
    mini_app_request_init(&req);
    req.method = MINI_APP_HTTP_GET;
    strcpy(req.uri, "/nonexistent");

    MiniAppHttpResponse res;
    mini_app_response_init(&res);
    mini_app_server_dispatch(&srv, &req, &res);
    CHECK(res.status == MINI_APP_HTTP_NOT_FOUND);
    mini_app_request_free(&req);
    free(res.body);
    PASS();
}

static void t05_url_router(void) {
    TEST("URL router pattern matching");
    MiniAppUrlRouter router;
    mini_app_router_init(&router);
    mini_app_router_add(&router, "/api/users", 1);
    mini_app_router_add(&router, "/api/users/:id", 2);
    mini_app_router_add(&router, "/api/*", 3);

    MiniAppUrlParam params[8];
    int h = mini_app_router_match(&router, "/api/users", params, 8);
    CHECK(h == 1);

    h = mini_app_router_match(&router, "/api/users/42", params, 8);
    CHECK(h == 2);

    h = mini_app_router_match(&router, "/api/unknown/path", params, 8);
    CHECK(h == 3);

    mini_app_router_free(&router);
    PASS();
}

static void t06_session_create_get(void) {
    TEST("Session create and retrieve");
    MiniAppSessionStore store;
    mini_app_session_store_init(&store, "super-secret-key-123");

    char *sid = mini_app_session_create(&store, "user123", 3600);
    CHECK(sid != NULL);

    MiniAppSession *s = mini_app_session_get(&store, sid);
    CHECK(s != NULL);
    CHECK(strcmp(s->user_id, "user123") == 0);

    bool valid = mini_app_session_validate(&store, sid);
    CHECK(valid);
    PASS();
}

static void t07_user_auth(void) {
    TEST("User password hashing and verification");
    MiniAppUserStore store;
    mini_app_user_store_init(&store);
    mini_app_user_add(&store, "admin", "secret123", 1);

    bool ok = mini_app_user_verify(&store, "admin", "secret123");
    CHECK(ok);

    ok = mini_app_user_verify(&store, "admin", "wrongpass");
    CHECK(!ok);

    ok = mini_app_user_verify(&store, "nonexistent", "secret123");
    CHECK(!ok);
    PASS();
}

static void t08_json_create_serialize(void) {
    TEST("JSON create and serialize");
    MiniAppJsonNode *obj = mini_app_json_create_object();
    mini_app_json_object_set(obj, "name", mini_app_json_create_string("Alice"));
    mini_app_json_object_set(obj, "age", mini_app_json_create_number(30));
    mini_app_json_object_set(obj, "active", mini_app_json_create_bool(true));

    MiniAppJsonNode *arr = mini_app_json_create_array();
    mini_app_json_array_add(arr, mini_app_json_create_string("admin"));
    mini_app_json_array_add(arr, mini_app_json_create_string("user"));
    mini_app_json_object_set(obj, "roles", arr);

    char buf[1024];
    int n = mini_app_json_serialize(obj, buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(strstr(buf, "\"name\"") != NULL);
    CHECK(strstr(buf, "\"Alice\"") != NULL);
    CHECK(strstr(buf, "\"age\":30") != NULL);
    CHECK(strstr(buf, "\"active\":true") != NULL);

    mini_app_json_free(obj);
    PASS();
}

static void t09_json_parse(void) {
    TEST("JSON parse");
    const char *json = "{\"id\":42,\"title\":\"Hello\",\"tags\":[\"a\",\"b\"]}";
    MiniAppJsonNode *root = mini_app_json_parse(json);
    CHECK(root != NULL);
    CHECK(root->type == MINI_APP_JSON_OBJECT);

    MiniAppJsonNode *id = mini_app_json_object_get(root, "id");
    CHECK(id != NULL);
    CHECK(id->type == MINI_APP_JSON_NUMBER);

    MiniAppJsonNode *title = mini_app_json_object_get(root, "title");
    CHECK(title != NULL);
    CHECK(strcmp(title->value.str_val, "Hello") == 0);

    mini_app_json_free(root);
    PASS();
}

static void t10_rate_limiter(void) {
    TEST("Token bucket rate limiter");
    MiniAppRateLimiter rl;
    mini_app_rate_limiter_init(&rl, 5.0, 1.0); /* 5 tokens max, 1/sec refill */

    /* Initial burst of 5 should all pass */
    for (int i = 0; i < 5; i++) {
        bool ok = mini_app_rate_limiter_allow(&rl, "client1");
        CHECK(ok);
    }
    /* 6th should fail (bucket empty) */
    bool ok = mini_app_rate_limiter_allow(&rl, "client1");
    CHECK(!ok);
    PASS();
}

static void t11_sliding_window(void) {
    TEST("Sliding window rate limiter");
    MiniAppSlidingWindow sw;
    mini_app_sliding_window_init(&sw, 60, 3); /* 3 requests per 60 seconds */

    bool ok = mini_app_sliding_window_allow(&sw, 100);
    CHECK(ok);
    ok = mini_app_sliding_window_allow(&sw, 100);
    CHECK(ok);
    ok = mini_app_sliding_window_allow(&sw, 100);
    CHECK(ok);
    /* 4th should fail */
    ok = mini_app_sliding_window_allow(&sw, 100);
    CHECK(!ok);
    PASS();
}

static void t12_token_auth(void) {
    TEST("JWT-like token creation and verification");
    char token[256];
    bool ok = mini_app_token_create("user99", "my-secret", 3600, token, sizeof(token));
    CHECK(ok);
    CHECK(strlen(token) > 10);
    CHECK(strchr(token, '.') != NULL);

    char uid[64];
    ok = mini_app_token_verify(token, "my-secret", uid, sizeof(uid));
    CHECK(ok);
    CHECK(strcmp(uid, "user99") == 0);

    /* Wrong secret should fail */
    ok = mini_app_token_verify(token, "wrong-secret", uid, sizeof(uid));
    CHECK(!ok);
    PASS();
}

static void t13_mime_type(void) {
    TEST("MIME type detection");
    CHECK(strcmp(mini_app_http_mime_type("index.html"), "text/html; charset=utf-8") == 0);
    CHECK(strcmp(mini_app_http_mime_type("style.css"), "text/css; charset=utf-8") == 0);
    CHECK(strcmp(mini_app_http_mime_type("app.js"), "application/javascript; charset=utf-8") == 0);
    CHECK(strcmp(mini_app_http_mime_type("photo.png"), "image/png") == 0);
    CHECK(strcmp(mini_app_http_mime_type("unknown.xyz"), "application/octet-stream") == 0);
    PASS();
}

static void t14_url_encode_decode(void) {
    TEST("URL encode/decode");
    char encoded[256], decoded[256];
    bool ok = mini_app_url_encode("hello world & more", encoded, sizeof(encoded));
    CHECK(ok);

    ok = mini_app_url_decode(encoded, decoded, sizeof(decoded));
    CHECK(ok);
    CHECK(strcmp(decoded, "hello world & more") == 0);
    PASS();
}

static void t15_session_cleanup(void) {
    TEST("Session cleanup of expired sessions");
    MiniAppSessionStore store;
    mini_app_session_store_init(&store, "cleanup-key");
    mini_app_session_create(&store, "user1", 0); /* Immediate expiry */

    mini_app_session_cleanup(&store);
    CHECK(store.session_count == 0);
    PASS();
}

static void t16_content_cache(void) {
    TEST("Content cache put and get");
    MiniAppContentCache cache;
    mini_app_cache_init(&cache, 1024 * 1024);

    const char *data = "Hello, cached world!";
    bool ok = mini_app_cache_put(&cache, "/page1",
                                  (const uint8_t *)data, strlen(data),
                                  3600, "text/plain");
    CHECK(ok);
    CHECK(mini_app_cache_has(&cache, "/page1"));

    uint8_t *cached_data;
    size_t cached_len;
    char ct[128];
    ok = mini_app_cache_get(&cache, "/page1", &cached_data, &cached_len, ct, sizeof(ct));
    CHECK(ok);
    CHECK(cached_len == strlen(data));
    CHECK(strncmp((const char *)cached_data, data, cached_len) == 0);

    uint64_t hits, misses, evictions;
    double hit_ratio;
    mini_app_cache_get_stats(&cache, &hits, &misses, &evictions, &hit_ratio);
    CHECK(hits == 1);

    mini_app_cache_clear(&cache);
    PASS();
}

static void t17_template_render(void) {
    TEST("Template engine render");
    MiniAppTemplateEngine tmpl;
    mini_app_template_init(&tmpl, "Hello, {{ name }}! You have {{ count }} messages.");
    mini_app_template_set(&tmpl, "name", "Alice");
    mini_app_template_set_int(&tmpl, "count", 5);

    char *result = mini_app_template_render(&tmpl);
    CHECK(result != NULL);
    CHECK(strstr(result, "Alice") != NULL);
    CHECK(strstr(result, "5") != NULL);

    mini_app_template_free(&tmpl);
    PASS();
}

static void t18_html_escape(void) {
    TEST("HTML escaping");
    char escaped[256];
    mini_app_html_escape("<script>alert('xss')</script>", escaped, sizeof(escaped));
    CHECK(strstr(escaped, "&lt;") != NULL);
    CHECK(strstr(escaped, "&gt;") != NULL);
    CHECK(strstr(escaped, "&#39;") != NULL);
    PASS();
}

static void t19_middleware(void) {
    TEST("Middleware chain");
    MiniAppMiddlewareChain chain;
    mini_app_middleware_chain_init(&chain);
    mini_app_middleware_add_logging(&chain);
    mini_app_middleware_add_cors(&chain);
    mini_app_middleware_add_security_headers(&chain);
    CHECK(chain.count == 3);
    PASS();
}

static void t20_websocket_upgrade(void) {
    TEST("WebSocket upgrade handshake");
    MiniAppHttpRequest req;
    mini_app_request_init(&req);
    req.method = MINI_APP_HTTP_GET;
    strcpy(req.uri, "/ws");
    strcpy(req.headers[0].key, "Upgrade");
    strcpy(req.headers[0].value, "websocket");
    strcpy(req.headers[1].key, "Sec-WebSocket-Key");
    strcpy(req.headers[1].value, "dGhlIHNhbXBsZSBub25jZQ==");
    req.header_count = 2;

    MiniAppHttpResponse res;
    mini_app_response_init(&res);

    bool ok = mini_app_websocket_upgrade(&req, &res);
    CHECK(ok);
    CHECK(res.status == 101);
    CHECK(mini_app_get_header(&req, "Upgrade") != NULL);

    mini_app_request_free(&req);
    free(res.body);
    PASS();
}

static void t21_format_number(void) {
    TEST("Number formatting with commas");
    char buf[64];
    mini_app_template_format_number(1234567, buf, sizeof(buf));
    CHECK(strcmp(buf, "1,234,567") == 0);
    PASS();
}

int main(void) {
    printf("=== mini-internet-app Test Suite ===\n\n");
    t01_request_parse();
    t02_response_serialize();
    t03_web_server_routing();
    t04_404_handler();
    t05_url_router();
    t06_session_create_get();
    t07_user_auth();
    t08_json_create_serialize();
    t09_json_parse();
    t10_rate_limiter();
    t11_sliding_window();
    t12_token_auth();
    t13_mime_type();
    t14_url_encode_decode();
    t15_session_cleanup();
    t16_content_cache();
    t17_template_render();
    t18_html_escape();
    t19_middleware();
    t20_websocket_upgrade();
    t21_format_number();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           passed, passed + failed, failed);
    return failed > 0 ? 1 : 0;
}
