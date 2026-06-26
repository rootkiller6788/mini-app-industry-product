#include "web_server.h"
#include "url_router.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

/* HTTP Method string mapping */
const char *mini_app_http_method_str(MiniAppHttpMethod m)
{
    switch (m) {
    case MINI_APP_HTTP_GET:     return "GET";
    case MINI_APP_HTTP_POST:    return "POST";
    case MINI_APP_HTTP_PUT:     return "PUT";
    case MINI_APP_HTTP_DELETE:  return "DELETE";
    case MINI_APP_HTTP_HEAD:    return "HEAD";
    case MINI_APP_HTTP_OPTIONS: return "OPTIONS";
    case MINI_APP_HTTP_PATCH:   return "PATCH";
    default:                    return "UNKNOWN";
    }
}

const char *mini_app_http_status_str(MiniAppHttpStatus s)
{
    switch (s) {
    case MINI_APP_HTTP_OK:                    return "200 OK";
    case MINI_APP_HTTP_CREATED:               return "201 Created";
    case MINI_APP_HTTP_NO_CONTENT:            return "204 No Content";
    case MINI_APP_HTTP_MOVED_PERMANENTLY:     return "301 Moved Permanently";
    case MINI_APP_HTTP_FOUND:                 return "302 Found";
    case MINI_APP_HTTP_NOT_MODIFIED:          return "304 Not Modified";
    case MINI_APP_HTTP_BAD_REQUEST:           return "400 Bad Request";
    case MINI_APP_HTTP_UNAUTHORIZED:          return "401 Unauthorized";
    case MINI_APP_HTTP_FORBIDDEN:             return "403 Forbidden";
    case MINI_APP_HTTP_NOT_FOUND:             return "404 Not Found";
    case MINI_APP_HTTP_METHOD_NOT_ALLOWED:    return "405 Method Not Allowed";
    case MINI_APP_HTTP_CONFLICT:              return "409 Conflict";
    case MINI_APP_HTTP_TOO_MANY_REQUESTS:     return "429 Too Many Requests";
    case MINI_APP_HTTP_INTERNAL_SERVER_ERROR: return "500 Internal Server Error";
    case MINI_APP_HTTP_SERVICE_UNAVAILABLE:   return "503 Service Unavailable";
    default:                                   return "500 Internal Server Error";
    }
}

static const struct { const char *ext; const char *mime; } mime_map[] = {
    {".html", "text/html; charset=utf-8"},
    {".htm",  "text/html; charset=utf-8"},
    {".css",  "text/css; charset=utf-8"},
    {".js",   "application/javascript; charset=utf-8"},
    {".json", "application/json; charset=utf-8"},
    {".xml",  "application/xml; charset=utf-8"},
    {".txt",  "text/plain; charset=utf-8"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".svg",  "image/svg+xml"},
    {".ico",  "image/x-icon"},
    {".pdf",  "application/pdf"},
    {".zip",  "application/zip"},
    {".gz",   "application/gzip"},
    {".tar",  "application/x-tar"},
    {".mp4",  "video/mp4"},
    {".webm", "video/webm"},
    {".mp3",  "audio/mpeg"},
    {".wav",  "audio/wav"},
    {".woff2","font/woff2"},
    {".wasm", "application/wasm"},
    {NULL,    "application/octet-stream"}
};

const char *mini_app_http_mime_type(const char *filename)
{
    if (!filename) return "application/octet-stream";
    const char *ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    for (int i = 0; mime_map[i].ext; i++) {
        if (strcasecmp(ext, mime_map[i].ext) == 0) {
            return mime_map[i].mime;
        }
    }
    return "application/octet-stream";
}

void mini_app_request_init(MiniAppHttpRequest *req)
{
    memset(req, 0, sizeof(*req));
    req->method = MINI_APP_HTTP_UNKNOWN;
    strcpy(req->version, "HTTP/1.1");
}

void mini_app_request_free(MiniAppHttpRequest *req)
{
    if (req && req->body) {
        free(req->body);
        req->body = NULL;
        req->body_len = 0;
    }
}

static MiniAppHttpMethod parse_method(const char *method_str)
{
    if (strcmp(method_str, "GET") == 0)     return MINI_APP_HTTP_GET;
    if (strcmp(method_str, "POST") == 0)    return MINI_APP_HTTP_POST;
    if (strcmp(method_str, "PUT") == 0)     return MINI_APP_HTTP_PUT;
    if (strcmp(method_str, "DELETE") == 0)  return MINI_APP_HTTP_DELETE;
    if (strcmp(method_str, "HEAD") == 0)    return MINI_APP_HTTP_HEAD;
    if (strcmp(method_str, "OPTIONS") == 0) return MINI_APP_HTTP_OPTIONS;
    if (strcmp(method_str, "PATCH") == 0)   return MINI_APP_HTTP_PATCH;
    return MINI_APP_HTTP_UNKNOWN;
}

int mini_app_request_parse(MiniAppHttpRequest *req, const uint8_t *data, size_t len)
{
    if (!req || !data || len == 0) return -1;

    char *buf = (char *)malloc(len + 1);
    if (!buf) return -1;
    memcpy(buf, data, len);
    buf[len] = '\0';

    char *ptr = buf;
    char *line_end;

    /* Parse request line: METHOD URI HTTP/1.1 */
    line_end = strstr(ptr, "\r\n");
    if (!line_end) { free(buf); return -1; }
    *line_end = '\0';

    char method_str[16] = {0}, uri[MINI_APP_HTTP_MAX_URI] = {0}, version[16] = {0};
    if (sscanf(ptr, "%15s %2047s %15s", method_str, uri, version) < 2) {
        free(buf); return -1;
    }
    req->method = parse_method(method_str);
    strncpy(req->uri, uri, MINI_APP_HTTP_MAX_URI - 1);
    if (version[0]) strncpy(req->version, version, MINI_APP_HTTP_MAX_VERSION - 1);

    ptr = line_end + 2;

    /* Parse headers */
    req->header_count = 0;
    while (req->header_count < MINI_APP_HTTP_MAX_HEADERS && *ptr != '\r' && *ptr != '\n' && *ptr != '\0') {
        line_end = strstr(ptr, "\r\n");
        if (!line_end || line_end == ptr) break;
        *line_end = '\0';

        char *colon = strchr(ptr, ':');
        if (colon) {
            *colon = '\0';
            char *value = colon + 1;
            while (*value == ' ' || *value == '\t') value++;
            strncpy(req->headers[req->header_count].key, ptr, 255);
            strncpy(req->headers[req->header_count].value, value, MINI_APP_HTTP_MAX_HEADER_LEN - 1);
            req->header_count++;
        }
        ptr = line_end + 2;
    }

    /* Skip blank line */
    if (*ptr == '\r') ptr++;
    if (*ptr == '\n') ptr++;

    /* Parse body if present */
    size_t remaining = len - (size_t)(ptr - buf);
    if (remaining > 0 && remaining < MINI_APP_HTTP_MAX_BODY) {
        req->body = (uint8_t *)malloc(remaining);
        if (req->body) {
            memcpy(req->body, ptr, remaining);
            req->body_len = remaining;
        }
    }

    free(buf);
    return 0;
}

void mini_app_response_init(MiniAppHttpResponse *res)
{
    memset(res, 0, sizeof(*res));
    res->status = MINI_APP_HTTP_OK;
    res->keep_alive = true;
}

void mini_app_response_set_body(MiniAppHttpResponse *res, const uint8_t *body, size_t len)
{
    if (res->body) free(res->body);
    res->body = (uint8_t *)malloc(len);
    if (res->body) {
        memcpy(res->body, body, len);
        res->body_len = len;
    }
}

void mini_app_response_add_header(MiniAppHttpResponse *res, const char *key, const char *value)
{
    if (res->header_count < MINI_APP_HTTP_MAX_HEADERS) {
        strncpy(res->headers[res->header_count].key, key, 255);
        strncpy(res->headers[res->header_count].value, value, MINI_APP_HTTP_MAX_HEADER_LEN - 1);
        res->header_count++;
    }
}

size_t mini_app_response_serialize(const MiniAppHttpResponse *res, uint8_t *buf, size_t bufsz)
{
    if (!res || !buf || bufsz == 0) return 0;

    char *out = (char *)buf;
    size_t written = 0;

    /* Status line */
    int n = snprintf(out, bufsz - written, "HTTP/1.1 %s\r\n",
                     mini_app_http_status_str(res->status));
    if (n < 0 || (size_t)n >= bufsz - written) return 0;
    written += (size_t)n;
    out += n;

    /* Headers */
    for (int i = 0; i < res->header_count; i++) {
        n = snprintf(out, bufsz - written, "%s: %s\r\n",
                     res->headers[i].key, res->headers[i].value);
        if (n < 0 || (size_t)n >= bufsz - written) return 0;
        written += (size_t)n;
        out += n;
    }

    /* Connection header */
    const char *conn = res->keep_alive ? "keep-alive" : "close";
    n = snprintf(out, bufsz - written, "Connection: %s\r\n", conn);
    if (n < 0 || (size_t)n >= bufsz - written) return 0;
    written += (size_t)n;
    out += n;

    /* Content-Length and body */
    if (res->body && res->body_len > 0) {
        n = snprintf(out, bufsz - written, "Content-Length: %zu\r\n\r\n", res->body_len);
        if (n < 0 || (size_t)n >= bufsz - written) return 0;
        written += (size_t)n;
        out += n;

        if (written + res->body_len > bufsz) return 0;
        memcpy(out, res->body, res->body_len);
        written += res->body_len;
    } else {
        n = snprintf(out, bufsz - written, "Content-Length: 0\r\n\r\n");
        if (n < 0 || (size_t)n >= bufsz - written) return 0;
        written += (size_t)n;
    }
    return written;
}

void mini_app_server_init(MiniAppWebServer *srv, const char *name)
{
    memset(srv, 0, sizeof(*srv));
    if (name) strncpy(srv->server_name, name, 127);
    srv->route_count = 0;
    srv->running = false;
}

int mini_app_server_add_route(MiniAppWebServer *srv, MiniAppHttpMethod method,
                               const char *pattern,
                               void (*handler)(const MiniAppHttpRequest *,
                                               MiniAppHttpResponse *, void *),
                               void *ctx)
{
    if (!srv || srv->route_count >= MINI_APP_MAX_ROUTES) return -1;

    MiniAppRoute *route = &srv->routes[srv->route_count];
    strncpy(route->method, mini_app_http_method_str(method), 15);
    strncpy(route->uri_pattern, pattern, 255);
    route->handler = handler;
    route->ctx = ctx;
    srv->route_count++;
    return srv->route_count - 1;
}

void mini_app_server_set_static(MiniAppWebServer *srv, const char *dir)
{
    if (srv && dir) strncpy(srv->static_dir, dir, 255);
}

static bool route_match(const MiniAppRoute *route, const MiniAppHttpRequest *req)
{
    /* Exact match */
    if (strcmp(route->uri_pattern, req->uri) == 0) return true;
    /* Method must match */
    if (strcmp(route->method, mini_app_http_method_str(req->method)) != 0) return false;
    /* Wildcard prefix */
    size_t plen = strlen(route->uri_pattern);
    if (plen > 0 && route->uri_pattern[plen - 1] == '*') {
        return strncmp(req->uri, route->uri_pattern, plen - 1) == 0;
    }
    return false;
}

int mini_app_server_dispatch(MiniAppWebServer *srv, const MiniAppHttpRequest *req,
                              MiniAppHttpResponse *res)
{
    if (!srv || !req || !res) return -1;

    /* Try registered routes first */
    for (int i = 0; i < srv->route_count; i++) {
        if (route_match(&srv->routes[i], req)) {
            srv->routes[i].handler(req, res, srv->routes[i].ctx);
            return 0;
        }
    }

    /* Static file serving */
    if (srv->static_dir[0] && req->method == MINI_APP_HTTP_GET) {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s%s", srv->static_dir, req->uri);
        /* Security: prevent directory traversal */
        if (strstr(req->uri, "..")) {
            res->status = MINI_APP_HTTP_FORBIDDEN;
            mini_app_response_set_body(res, (const uint8_t *)"Forbidden", 9);
            return 0;
        }
        FILE *f = fopen(filepath, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (fsize > 0) {
                uint8_t *fbuf = (uint8_t *)malloc((size_t)fsize);
                if (fbuf) {
                    fread(fbuf, 1, (size_t)fsize, f);
                    mini_app_response_set_body(res, fbuf, (size_t)fsize);
                    free(fbuf);
                    char content_len[64];
                    snprintf(content_len, sizeof(content_len), "%ld", fsize);
                    mini_app_response_add_header(res, "Content-Type",
                                                  mini_app_http_mime_type(filepath));
                    mini_app_response_add_header(res, "Content-Length", content_len);
                }
            }
            fclose(f);
            return 0;
        }
    }

    /* 404 Not Found */
    res->status = MINI_APP_HTTP_NOT_FOUND;
    const char *body_404 = "{\"error\":\"Not Found\",\"code\":404}";
    mini_app_response_set_body(res, (const uint8_t *)body_404, strlen(body_404));
    mini_app_response_add_header(res, "Content-Type", "application/json");
    return 0;
}

/* ============================================================================
 * L7: Middleware Pipeline
 *
 * Middleware wraps route handlers with cross-cutting concerns.
 * ========================================================================== */

void mini_app_middleware_chain_init(MiniAppMiddlewareChain *chain)
{
    memset(chain, 0, sizeof(*chain));
}

int mini_app_middleware_add(MiniAppMiddlewareChain *chain,
                             const char *name,
                             bool (*pre)(const MiniAppHttpRequest *, MiniAppHttpResponse *, void *),
                             void (*post)(const MiniAppHttpRequest *, MiniAppHttpResponse *, void *),
                             void *ctx)
{
    if (!chain || chain->count >= MINI_APP_MAX_MIDDLEWARE) return -1;
    MiniAppMiddleware *mw = &chain->middlewares[chain->count];
    strncpy(mw->name, name, 63);
    mw->pre = pre;
    mw->post = post;
    mw->ctx = ctx;
    chain->count++;
    return chain->count - 1;
}

int mini_app_middleware_run_pre(MiniAppMiddlewareChain *chain,
                                 const MiniAppHttpRequest *req,
                                 MiniAppHttpResponse *res)
{
    for (int i = 0; i < chain->count; i++) {
        if (chain->middlewares[i].pre) {
            if (!chain->middlewares[i].pre(req, res, chain->middlewares[i].ctx)) {
                return i; /* Middleware rejected request */
            }
        }
    }
    return -1; /* All passed */
}

void mini_app_middleware_run_post(MiniAppMiddlewareChain *chain,
                                   const MiniAppHttpRequest *req,
                                   MiniAppHttpResponse *res)
{
    for (int i = chain->count - 1; i >= 0; i--) {
        if (chain->middlewares[i].post) {
            chain->middlewares[i].post(req, res, chain->middlewares[i].ctx);
        }
    }
}

/* Built-in middleware: Request logging */
static bool logging_mw_pre(const MiniAppHttpRequest *req, MiniAppHttpResponse *res, void *ctx)
{
    (void)res; (void)ctx;
    printf("[LOG] %s %s from %s\n",
           mini_app_http_method_str(req->method), req->uri, req->remote_addr);
    return true;
}

static void logging_mw_post(const MiniAppHttpRequest *req, MiniAppHttpResponse *res, void *ctx)
{
    (void)ctx;
    printf("[LOG] %s %s → %s (%zu bytes)\n",
           mini_app_http_method_str(req->method), req->uri,
           mini_app_http_status_str(res->status), res->body_len);
}

void mini_app_middleware_add_logging(MiniAppMiddlewareChain *chain)
{
    mini_app_middleware_add(chain, "logging", logging_mw_pre, logging_mw_post, NULL);
}

/* Built-in middleware: CORS headers */
static bool cors_mw_pre(const MiniAppHttpRequest *req, MiniAppHttpResponse *res, void *ctx)
{
    (void)ctx;
    mini_app_response_add_header(res, "Access-Control-Allow-Origin", "*");
    mini_app_response_add_header(res, "Access-Control-Allow-Methods",
                                  "GET, POST, PUT, DELETE, OPTIONS");
    mini_app_response_add_header(res, "Access-Control-Allow-Headers",
                                  "Content-Type, Authorization");

    if (req->method == MINI_APP_HTTP_OPTIONS) {
        res->status = MINI_APP_HTTP_NO_CONTENT;
        return false; /* Stop chain, return preflight response */
    }
    return true;
}

void mini_app_middleware_add_cors(MiniAppMiddlewareChain *chain)
{
    mini_app_middleware_add(chain, "cors", cors_mw_pre, NULL, NULL);
}

/* Built-in middleware: Security headers */
static bool security_mw_pre(const MiniAppHttpRequest *req, MiniAppHttpResponse *res, void *ctx)
{
    (void)req; (void)ctx;
    mini_app_response_add_header(res, "X-Content-Type-Options", "nosniff");
    mini_app_response_add_header(res, "X-Frame-Options", "DENY");
    mini_app_response_add_header(res, "X-XSS-Protection", "1; mode=block");
    mini_app_response_add_header(res, "Referrer-Policy", "strict-origin-when-cross-origin");
    mini_app_response_add_header(res, "Permissions-Policy",
                                  "camera=(), microphone=(), geolocation=()");
    return true;
}

void mini_app_middleware_add_security_headers(MiniAppMiddlewareChain *chain)
{
    mini_app_middleware_add(chain, "security", security_mw_pre, NULL, NULL);
}

/* ============================================================================
 * L7: Request Body Parsing Utilities
 *
 * Common operations for REST APIs:
 * - URL query string parsing
 * - Form URL-encoded body parsing
 * - Multipart form data (file upload)
 * - Content negotiation (Accept header)
 * ========================================================================== */

int mini_app_parse_query_string(const char *uri, MiniAppQueryParam *params, int max_params)
{
    const char *q = strchr(uri, '?');
    if (!q) return 0;
    q++;

    int count = 0;
    char work[2048];
    strncpy(work, q, 2047);
    work[2047] = '\0';

    char *saveptr = NULL;
    char *pair = strtok_r(work, "&", &saveptr);
    while (pair && count < max_params) {
        char *eq = strchr(pair, '=');
        if (eq) {
            *eq = '\0';
            strncpy(params[count].key, pair, 127);
            mini_app_url_decode(eq + 1, params[count].value, 511);
        } else {
            strncpy(params[count].key, pair, 127);
            params[count].value[0] = '\0';
        }
        count++;
        pair = strtok_r(NULL, "&", &saveptr);
    }
    return count;
}

const char *mini_app_get_header(const MiniAppHttpRequest *req, const char *key)
{
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].key, key) == 0) {
            return req->headers[i].value;
        }
    }
    return NULL;
}

bool mini_app_accepts(const MiniAppHttpRequest *req, const char *mime_type)
{
    const char *accept = mini_app_get_header(req, "Accept");
    if (!accept || strcmp(accept, "*/*") == 0) return true;
    return strstr(accept, mime_type) != NULL;
}

/* ============================================================================
 * L6: Connection Pool & Keep-Alive Management
 *
 * HTTP persistent connections (keep-alive) reduce TCP handshake overhead.
 * The server maintains a pool of active connections with idle timeouts.
 *
 * Connection lifecycle:
 * 1. Accept TCP connection
 * 2. Read HTTP request(s) from connection
 * 3. Process each request, check keep-alive
 * 4. Send response with Connection header
 * 5. If keep-alive: wait for next request (with idle timeout)
 * 6. If close: shutdown TCP connection
 *
 * L4 Standard: RFC 7230 §6.3 - Persistence
 * Default idle timeout: 5 seconds for HTTP/1.1 keep-alive
 * ========================================================================== */

#define MINI_APP_MAX_CONNECTIONS 256

typedef struct {
    int    fd;
    char   remote_addr[64];
    uint16_t remote_port;
    uint64_t last_activity;
    bool   keep_alive;
    int    requests_served;
    uint8_t recv_buf[65536];
    size_t recv_len;
} MiniAppConnection;

typedef struct {
    MiniAppConnection connections[MINI_APP_MAX_CONNECTIONS];
    int    active_count;
    uint64_t idle_timeout_ms;
} MiniAppConnectionPool;

void mini_app_conn_pool_init(MiniAppConnectionPool *pool, uint64_t idle_timeout_ms)
{
    memset(pool, 0, sizeof(*pool));
    pool->idle_timeout_ms = idle_timeout_ms;
}

int mini_app_conn_pool_add(MiniAppConnectionPool *pool, int fd,
                            const char *remote_addr, uint16_t remote_port)
{
    if (pool->active_count >= MINI_APP_MAX_CONNECTIONS) return -1;
    MiniAppConnection *conn = &pool->connections[pool->active_count];
    memset(conn, 0, sizeof(*conn));
    conn->fd = fd;
    if (remote_addr) strncpy(conn->remote_addr, remote_addr, 63);
    conn->remote_port = remote_port;
    conn->last_activity = (uint64_t)time(NULL) * 1000;
    conn->keep_alive = true;
    pool->active_count++;
    return pool->active_count - 1;
}

void mini_app_conn_pool_remove(MiniAppConnectionPool *pool, int idx)
{
    if (idx < 0 || idx >= pool->active_count) return;
    /* Shift remaining connections */
    for (int i = idx; i < pool->active_count - 1; i++) {
        memcpy(&pool->connections[i], &pool->connections[i + 1],
               sizeof(MiniAppConnection));
    }
    pool->active_count--;
}

void mini_app_conn_pool_cleanup_idle(MiniAppConnectionPool *pool)
{
    uint64_t now = (uint64_t)time(NULL) * 1000;
    int i = 0;
    while (i < pool->active_count) {
        if (now - pool->connections[i].last_activity > pool->idle_timeout_ms) {
            mini_app_conn_pool_remove(pool, i);
        } else {
            i++;
        }
    }
}

/* ============================================================================
 * L7: Content Compression
 *
 * HTTP response compression reduces bandwidth usage.
 * Supported algorithms: gzip (RFC 1952), deflate (RFC 1951)
 *
 * Compression negotiation:
 * 1. Client sends Accept-Encoding: gzip, deflate
 * 2. Server selects best common algorithm
 * 3. Server adds Content-Encoding header to response
 *
 * This implementation provides the API structure with documentation.
 * Actual compression would use zlib (not included to keep dependency-free).
 * ========================================================================== */

MiniAppCompressAlgo mini_app_negotiate_encoding(const MiniAppHttpRequest *req)
{
    const char *ae = mini_app_get_header(req, "Accept-Encoding");
    if (!ae) return MINI_APP_COMPRESS_NONE;
    if (strstr(ae, "gzip"))    return MINI_APP_COMPRESS_GZIP;
    if (strstr(ae, "deflate")) return MINI_APP_COMPRESS_DEFLATE;
    return MINI_APP_COMPRESS_NONE;
}

const char *mini_app_compress_algo_str(MiniAppCompressAlgo algo)
{
    switch (algo) {
    case MINI_APP_COMPRESS_GZIP:    return "gzip";
    case MINI_APP_COMPRESS_DEFLATE: return "deflate";
    default:                        return "identity";
    }
}

/* ============================================================================
 * L7: Server-Sent Events (SSE) Support
 *
 * SSE enables server-to-client streaming of events over HTTP.
 * Uses text/event-stream MIME type with keep-alive connections.
 *
 * Event format:
 *   id: <event_id>\n
 *   event: <event_type>\n
 *   data: <payload>\n\n
 *
 * Reference: W3C Server-Sent Events (SSE) specification
 * ========================================================================== */

void mini_app_sse_begin(MiniAppHttpResponse *res)
{
    mini_app_response_add_header(res, "Content-Type", "text/event-stream");
    mini_app_response_add_header(res, "Cache-Control", "no-cache");
    mini_app_response_add_header(res, "Connection", "keep-alive");
    res->keep_alive = true;
}

void mini_app_sse_send_event(MiniAppHttpResponse *res, const char *event_type,
                              const char *data)
{
    char buf[2048];
    int n = snprintf(buf, sizeof(buf), "event: %s\ndata: %s\n\n", event_type, data);
    if (n > 0) {
        mini_app_response_set_body(res, (const uint8_t *)buf, (size_t)n);
    }
}

/* ============================================================================
 * L8: WebSocket Upgrade Handshake (RFC 6455)
 *
 * WebSocket upgrade: HTTP GET with Upgrade: websocket header.
 * Server responds with 101 Switching Protocols.
 * After handshake, bidirectional TCP socket for message frames.
 *
 * Handshake security:
 * - Client sends Sec-WebSocket-Key (base64 random 16 bytes)
 * - Server responds with Sec-WebSocket-Accept =
 *   base64(sha1(client_key || "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
 * - This proves the server understands WebSocket, preventing
 *   cross-protocol attacks against non-WebSocket servers.
 * ========================================================================== */

/* Simple password strength checker used during registration */
int mini_app_password_strength(const char *password)
{
    if (!password) return 0;
    int score = 0;
    size_t len = strlen(password);
    if (len >= 8) score += 1;
    if (len >= 12) score += 1;
    if (len >= 16) score += 1;
    bool has_upper = false, has_lower = false, has_digit = false, has_special = false;
    for (size_t i = 0; i < len; i++) {
        if (isupper((unsigned char)password[i])) has_upper = true;
        else if (islower((unsigned char)password[i])) has_lower = true;
        else if (isdigit((unsigned char)password[i])) has_digit = true;
        else has_special = true;
    }
    if (has_upper) score++;
    if (has_lower) score++;
    if (has_digit) score++;
    if (has_special) score++;
    return score; /* 0-7, >=5 is strong */
}

bool mini_app_websocket_upgrade(const MiniAppHttpRequest *req,
                                 MiniAppHttpResponse *res)
{
    const char *upgrade = mini_app_get_header(req, "Upgrade");
    const char *ws_key = mini_app_get_header(req, "Sec-WebSocket-Key");

    if (!upgrade || strcasecmp(upgrade, "websocket") != 0) return false;
    if (!ws_key) return false;

    /* Compute accept key (simplified hash for educational purposes)
     * In production: SHA1(client_key || magic_string) → base64 */
    char accept_key[64];
    const char *magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    /* Simplified DJB2-style hash for WebSocket accept key computation */
    uint64_t h = 5381;
    for (const char *s = ws_key; *s; s++) h = ((h << 5) + h) + (unsigned char)*s;
    for (const char *s = magic; *s; s++) h = ((h << 5) + h) + (unsigned char)*s;

    snprintf(accept_key, sizeof(accept_key), "%016llx", (unsigned long long)h);

    res->status = (MiniAppHttpStatus)101; /* 101 Switching Protocols */
    mini_app_response_add_header(res, "Upgrade", "websocket");
    mini_app_response_add_header(res, "Connection", "Upgrade");
    mini_app_response_add_header(res, "Sec-WebSocket-Accept", accept_key);
    return true;
}

/* ============================================================================
 * L8: WebSocket Frame Processing (RFC 6455 §5)
 *
 * After upgrade, communication uses a binary frame protocol:
 *
 * Frame format (base framing, 2-14 bytes header):
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-------+-+-------------+-------------------------------+
 *  |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 *  |I|S|S|S|  (4)  |A|     (7)     |           (16/64)             |
 *  |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 *  | |1|2|3|       |K|             |                               |
 *  +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - -+
 *  |     Masking-key (if MASK set to 1) |      Payload Data        |
 *  +------------------------------------+ - - - - - - - - - - - - -+
 *
 * Opcodes: 0=continuation, 1=text, 2=binary, 8=close, 9=ping, 10=pong
 *
 * Masking: client→server frames MUST be masked (4-byte random key XOR).
 * server→client frames MUST NOT be masked.
 *
 * L4: RFC 6455 §5.3 — Client-to-Server Masking
 * Masking prevents cache poisoning attacks on intermediary proxies.
 * Without masking, an attacker could inject WebSocket frames that
 * a proxy interprets as HTTP responses (CVE-2010-4490).
 * ========================================================================== */

#define MINI_APP_WS_OPCODE_CONT  0x0
#define MINI_APP_WS_OPCODE_TEXT  0x1
#define MINI_APP_WS_OPCODE_BIN   0x2
#define MINI_APP_WS_OPCODE_CLOSE 0x8
#define MINI_APP_WS_OPCODE_PING  0x9
#define MINI_APP_WS_OPCODE_PONG  0xA

#define MINI_APP_WS_FIN_BIT      0x80
#define MINI_APP_WS_MASK_BIT     0x80
#define MINI_APP_WS_MAX_FRAME    65536

typedef struct {
    bool    fin;
    uint8_t opcode;
    uint8_t mask_key[4];
    bool    masked;
    uint64_t payload_len;
    uint8_t payload[MINI_APP_WS_MAX_FRAME];
} MiniAppWSFrame;

/* Encode a WebSocket frame for sending (server→client: unmasked) */
int mini_app_ws_encode_frame(uint8_t opcode, const uint8_t *data,
                               size_t len, uint8_t *frame_buf, size_t buf_sz)
{
    if (!frame_buf || buf_sz < 2) return -1;
    size_t pos = 0;
    frame_buf[pos++] = MINI_APP_WS_FIN_BIT | (opcode & 0x0F);

    if (len <= 125) {
        frame_buf[pos++] = (uint8_t)(len & 0x7F); /* MASK=0 */
    } else if (len <= 65535) {
        frame_buf[pos++] = 126; /* MASK=0, extended 16-bit */
        if (pos + 2 > buf_sz) return -1;
        frame_buf[pos++] = (uint8_t)((len >> 8) & 0xFF);
        frame_buf[pos++] = (uint8_t)(len & 0xFF);
    } else {
        frame_buf[pos++] = 127; /* MASK=0, extended 64-bit */
        if (pos + 8 > buf_sz) return -1;
        for (int i = 7; i >= 0; i--)
            frame_buf[pos++] = (uint8_t)((len >> (i * 8)) & 0xFF);
    }

    if (pos + len > buf_sz) return -1;
    if (data && len > 0) {
        memcpy(frame_buf + pos, data, len);
        pos += len;
    }
    return (int)pos;
}

/* Decode a WebSocket frame from client (client→server: masked) */
int mini_app_ws_decode_frame(const uint8_t *data, size_t len,
                               MiniAppWSFrame *frame)
{
    if (!data || !frame || len < 2) return -1;
    memset(frame, 0, sizeof(*frame));

    size_t pos = 0;
    uint8_t byte0 = data[pos++];
    frame->fin    = (byte0 & MINI_APP_WS_FIN_BIT) ? true : false;
    frame->opcode = byte0 & 0x0F;

    uint8_t byte1 = data[pos++];
    frame->masked = (byte1 & MINI_APP_WS_MASK_BIT) ? true : false;
    uint64_t plen = byte1 & 0x7F;

    if (plen == 126) {
        if (pos + 2 > len) return -1;
        plen = ((uint64_t)data[pos] << 8) | data[pos + 1];
        pos += 2;
    } else if (plen == 127) {
        if (pos + 8 > len) return -1;
        plen = 0;
        for (int i = 0; i < 8; i++)
            plen = (plen << 8) | data[pos + i];
        pos += 8;
    }

    frame->payload_len = plen;

    if (frame->masked) {
        if (pos + 4 > len) return -1;
        memcpy(frame->mask_key, data + pos, 4);
        pos += 4;
    }

    if (pos + plen > len || plen > MINI_APP_WS_MAX_FRAME) return -1;
    memcpy(frame->payload, data + pos, (size_t)plen);

    /* Unmask if client-sent */
    if (frame->masked) {
        for (uint64_t i = 0; i < plen; i++) {
            frame->payload[i] ^= frame->mask_key[i % 4];
        }
    }

    return (int)(pos + plen);
}

const char *mini_app_ws_opcode_name(uint8_t opcode)
{
    switch (opcode) {
    case MINI_APP_WS_OPCODE_CONT:  return "continuation";
    case MINI_APP_WS_OPCODE_TEXT:  return "text";
    case MINI_APP_WS_OPCODE_BIN:   return "binary";
    case MINI_APP_WS_OPCODE_CLOSE: return "close";
    case MINI_APP_WS_OPCODE_PING:  return "ping";
    case MINI_APP_WS_OPCODE_PONG:  return "pong";
    default:                       return "reserved";
    }
}

/* ============================================================================
 * L9: HTTP/2 Binary Framing Layer (RFC 7540)
 *
 * HTTP/2 introduces a binary framing layer for multiplexing:
 * - Multiple streams over single TCP connection
 * - Header compression (HPACK, RFC 7541)
 * - Server push (PUSH_PROMISE frames)
 * - Stream prioritization and flow control
 *
 * Frame format (9-byte header):
 *  +-----------------------------------------------+
 *  |        Length (24)        |  Type (8) |Flags(8)|
 *  +-----------------------------------------------+
 *  |R|              Stream Identifier (31)          |
 *  +=+=============================================+
 *  |                Frame Payload ...               |
 *  +-----------------------------------------------+
 *
 * Frame types: DATA(0x0), HEADERS(0x1), PRIORITY(0x2),
 * RST_STREAM(0x3), SETTINGS(0x4), PUSH_PROMISE(0x5),
 * PING(0x6), GOAWAY(0x7), WINDOW_UPDATE(0x8), CONTINUATION(0x9)
 *
 * Connection Preface: "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
 * followed by a SETTINGS frame.
 *
 * L4 Standard: RFC 7540 — Hypertext Transfer Protocol Version 2
 * CMU 15-441: Computer Networks — HTTP/2 multiplexing
 * ========================================================================== */

#define MINI_APP_H2_FRAME_DATA          0x0
#define MINI_APP_H2_FRAME_HEADERS       0x1
#define MINI_APP_H2_FRAME_PRIORITY      0x2
#define MINI_APP_H2_FRAME_RST_STREAM    0x3
#define MINI_APP_H2_FRAME_SETTINGS      0x4
#define MINI_APP_H2_FRAME_PUSH_PROMISE  0x5
#define MINI_APP_H2_FRAME_PING          0x6
#define MINI_APP_H2_FRAME_GOAWAY       0x7
#define MINI_APP_H2_FRAME_WINDOW_UPDATE 0x8
#define MINI_APP_H2_FRAME_CONTINUATION  0x9

#define MINI_APP_H2_PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"

#define MINI_APP_H2_MAX_FRAME_SIZE 16384
#define MINI_APP_H2_MAX_STREAMS    256

typedef struct {
    uint32_t length;       /* 24-bit */
    uint8_t  type;
    uint8_t  flags;
    uint32_t stream_id;    /* 31-bit */
    uint8_t  payload[MINI_APP_H2_MAX_FRAME_SIZE];
} MiniAppH2Frame;

typedef struct {
    uint32_t stream_id;
    uint8_t  state;        /* idle, open, half-closed, closed */
    uint32_t window_size;
    uint32_t peer_window;
    int      priority;
} MiniAppH2Stream;

/* Encode an HTTP/2 frame header */
int mini_app_h2_encode_frame(uint32_t length, uint8_t type, uint8_t flags,
                               uint32_t stream_id, uint8_t *buf, size_t buf_sz)
{
    if (!buf || buf_sz < 9) return -1;
    buf[0] = (uint8_t)((length >> 16) & 0xFF);
    buf[1] = (uint8_t)((length >> 8) & 0xFF);
    buf[2] = (uint8_t)(length & 0xFF);
    buf[3] = type;
    buf[4] = flags;
    buf[5] = (uint8_t)((stream_id >> 24) & 0x7F); /* R+31bit */
    buf[6] = (uint8_t)((stream_id >> 16) & 0xFF);
    buf[7] = (uint8_t)((stream_id >> 8) & 0xFF);
    buf[8] = (uint8_t)(stream_id & 0xFF);
    return 9;
}

const char *mini_app_h2_frame_type_name(uint8_t type)
{
    switch (type) {
    case MINI_APP_H2_FRAME_DATA:          return "DATA";
    case MINI_APP_H2_FRAME_HEADERS:       return "HEADERS";
    case MINI_APP_H2_FRAME_PRIORITY:      return "PRIORITY";
    case MINI_APP_H2_FRAME_RST_STREAM:    return "RST_STREAM";
    case MINI_APP_H2_FRAME_SETTINGS:      return "SETTINGS";
    case MINI_APP_H2_FRAME_PUSH_PROMISE:  return "PUSH_PROMISE";
    case MINI_APP_H2_FRAME_PING:          return "PING";
    case MINI_APP_H2_FRAME_GOAWAY:       return "GOAWAY";
    case MINI_APP_H2_FRAME_WINDOW_UPDATE: return "WINDOW_UPDATE";
    case MINI_APP_H2_FRAME_CONTINUATION:  return "CONTINUATION";
    default:                              return "UNKNOWN";
    }
}

/* Check if client sent HTTP/2 connection preface */
bool mini_app_h2_check_preface(const uint8_t *data, size_t len)
{
    if (len < 24) return false;
    return memcmp(data, MINI_APP_H2_PREFACE, 24) == 0;
}

/* Simulate HTTP/2 SETTINGS frame exchange */
int mini_app_h2_send_settings(uint8_t *buf, size_t buf_sz)
{
    /* SETTINGS frame with:
     * SETTINGS_MAX_CONCURRENT_STREAMS = 100  (0x3, value=100)
     * SETTINGS_INITIAL_WINDOW_SIZE = 65535    (0x4, value=65535)
     * SETTINGS_HEADER_TABLE_SIZE = 4096       (0x1, value=4096) */
    uint8_t payload[] = {
        0x00, 0x03, 0x00, 0x00, 0x00, 0x64, /* MAX_CONCURRENT_STREAMS=100 */
        0x00, 0x04, 0x00, 0x00, 0xFF, 0xFF, /* INITIAL_WINDOW_SIZE=65535 */
        0x00, 0x01, 0x00, 0x00, 0x10, 0x00  /* HEADER_TABLE_SIZE=4096 */
    };
    int hdr_len = mini_app_h2_encode_frame(sizeof(payload),
                                             MINI_APP_H2_FRAME_SETTINGS,
                                             0x00, 0x00, buf, buf_sz);
    if (hdr_len < 0 || (size_t)hdr_len + sizeof(payload) > buf_sz) return -1;
    memcpy(buf + hdr_len, payload, sizeof(payload));
    return hdr_len + (int)sizeof(payload);
}

/* ============================================================================
 * L9: HTTP/3 QUIC Overview
 *
 * HTTP/3 (RFC 9114) uses QUIC (RFC 9000) over UDP instead of TCP:
 * - 0-RTT connection establishment (vs 1.5-RTT for TCP+TLS)
 * - No head-of-line blocking (streams are independent)
 * - Connection migration (survives IP address changes)
 * - Built-in TLS 1.3 encryption (no separate handshake)
 *
 * This module documents the transition:
 * HTTP/1.1 (1997) → HTTP/2 (2015) → HTTP/3 (2022)
 *
 * Reference: RFC 9114, RFC 9000, RFC 9001
 * Stanford CS 244: Advanced Topics in Networking
 * ========================================================================== */

const char *mini_app_http_version_name(int major, int minor)
{
    if (major == 1 && minor == 0) return "HTTP/1.0";
    if (major == 1 && minor == 1) return "HTTP/1.1";
    if (major == 2 && minor == 0) return "HTTP/2";
    if (major == 3 && minor == 0) return "HTTP/3";
    return "HTTP/?.?";
}

/* RFC 6585 additional HTTP status codes */
int mini_app_http_too_early(MiniAppHttpResponse *res)
{
    /* 425 Too Early — server is unwilling to risk processing a request
     * that might be replayed (used with 0-RTT in TLS 1.3) */
    res->status = (MiniAppHttpStatus)425;
    const char *body = "{\"error\":\"Too Early\"}";
    mini_app_response_set_body(res, (const uint8_t *)body, strlen(body));
    return 425;
}

int mini_app_http_upgrade_required(MiniAppHttpResponse *res, const char *protocol)
{
    /* 426 Upgrade Required — client must switch to the given protocol */
    res->status = (MiniAppHttpStatus)426;
    char header_val[128];
    snprintf(header_val, sizeof(header_val), "%s", protocol);
    mini_app_response_add_header(res, "Upgrade", header_val);
    const char *body = "{\"error\":\"Upgrade Required\"}";
    mini_app_response_set_body(res, (const uint8_t *)body, strlen(body));
    return 426;
}

