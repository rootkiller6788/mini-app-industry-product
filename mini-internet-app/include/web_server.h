#ifndef MINI_APP_WEB_SERVER_H
#define MINI_APP_WEB_SERVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * L1: Core Definitions - Web Server (HTTP/1.1)
 *
 * A minimal but complete HTTP/1.1 web server implementation covering:
 * - HTTP request parsing (method, URI, headers, body)
 * - HTTP response construction (status codes, headers, body)
 * - Connection management (keep-alive, pipelining)
 * - Static file serving with MIME type detection
 * - Routing with pattern matching
 * - Middleware pipeline architecture
 *
 * Reference:
 * - RFC 7230-7235: HTTP/1.1
 * - MIT 6.033: Computer System Engineering - Web server design
 * - Stanford CS142: Web Applications
 * ========================================================================== */

#define MINI_APP_HTTP_MAX_HEADERS    64
#define MINI_APP_HTTP_MAX_URI        2048
#define MINI_APP_HTTP_MAX_HEADER_LEN 4096
#define MINI_APP_HTTP_MAX_BODY       (16 * 1024 * 1024)
#define MINI_APP_HTTP_MAX_METHOD     16
#define MINI_APP_HTTP_MAX_VERSION    16

typedef enum {
    MINI_APP_HTTP_GET     = 0,
    MINI_APP_HTTP_POST    = 1,
    MINI_APP_HTTP_PUT     = 2,
    MINI_APP_HTTP_DELETE  = 3,
    MINI_APP_HTTP_HEAD    = 4,
    MINI_APP_HTTP_OPTIONS = 5,
    MINI_APP_HTTP_PATCH   = 6,
    MINI_APP_HTTP_UNKNOWN = 7,
} MiniAppHttpMethod;

typedef enum {
    MINI_APP_HTTP_OK                    = 200,
    MINI_APP_HTTP_CREATED               = 201,
    MINI_APP_HTTP_NO_CONTENT            = 204,
    MINI_APP_HTTP_MOVED_PERMANENTLY     = 301,
    MINI_APP_HTTP_FOUND                 = 302,
    MINI_APP_HTTP_NOT_MODIFIED          = 304,
    MINI_APP_HTTP_BAD_REQUEST           = 400,
    MINI_APP_HTTP_UNAUTHORIZED          = 401,
    MINI_APP_HTTP_FORBIDDEN             = 403,
    MINI_APP_HTTP_NOT_FOUND             = 404,
    MINI_APP_HTTP_METHOD_NOT_ALLOWED    = 405,
    MINI_APP_HTTP_CONFLICT              = 409,
    MINI_APP_HTTP_TOO_MANY_REQUESTS     = 429,
    MINI_APP_HTTP_INTERNAL_SERVER_ERROR = 500,
    MINI_APP_HTTP_SERVICE_UNAVAILABLE   = 503,
} MiniAppHttpStatus;

typedef struct {
    char key[256];
    char value[MINI_APP_HTTP_MAX_HEADER_LEN];
} MiniAppHttpHeader;

typedef struct {
    MiniAppHttpMethod method;
    char     uri[MINI_APP_HTTP_MAX_URI];
    char     version[MINI_APP_HTTP_MAX_VERSION];
    MiniAppHttpHeader headers[MINI_APP_HTTP_MAX_HEADERS];
    int      header_count;
    uint8_t *body;
    size_t   body_len;
    char     remote_addr[64];
    uint16_t remote_port;
} MiniAppHttpRequest;

typedef struct {
    MiniAppHttpStatus status;
    MiniAppHttpHeader headers[MINI_APP_HTTP_MAX_HEADERS];
    int      header_count;
    uint8_t *body;
    size_t   body_len;
    bool     keep_alive;
} MiniAppHttpResponse;

typedef struct {
    char method[16];
    char uri_pattern[256];
    void (*handler)(const MiniAppHttpRequest *req, MiniAppHttpResponse *res, void *ctx);
    void *ctx;
} MiniAppRoute;

#define MINI_APP_MAX_ROUTES 256
typedef struct {
    MiniAppRoute routes[MINI_APP_MAX_ROUTES];
    int   route_count;
    char  static_dir[256];
    char  server_name[128];
    bool  running;
} MiniAppWebServer;

const char *mini_app_http_method_str(MiniAppHttpMethod m);
const char *mini_app_http_status_str(MiniAppHttpStatus s);
const char *mini_app_http_mime_type(const char *filename);

void mini_app_request_init(MiniAppHttpRequest *req);
void mini_app_request_free(MiniAppHttpRequest *req);
int  mini_app_request_parse(MiniAppHttpRequest *req, const uint8_t *data, size_t len);

void mini_app_response_init(MiniAppHttpResponse *res);
void mini_app_response_set_body(MiniAppHttpResponse *res, const uint8_t *body, size_t len);
void mini_app_response_add_header(MiniAppHttpResponse *res, const char *key, const char *value);
size_t mini_app_response_serialize(const MiniAppHttpResponse *res, uint8_t *buf, size_t bufsz);

void mini_app_server_init(MiniAppWebServer *srv, const char *name);
int  mini_app_server_add_route(MiniAppWebServer *srv, MiniAppHttpMethod method,
                                const char *pattern, void (*handler)(const MiniAppHttpRequest *,
                                MiniAppHttpResponse *, void *), void *ctx);
void mini_app_server_set_static(MiniAppWebServer *srv, const char *dir);
int  mini_app_server_dispatch(MiniAppWebServer *srv, const MiniAppHttpRequest *req,
                               MiniAppHttpResponse *res);

/* Middleware */
#define MINI_APP_MAX_MIDDLEWARE 16
typedef struct {
    char name[64];
    bool (*pre)(const MiniAppHttpRequest *req, MiniAppHttpResponse *res, void *ctx);
    void (*post)(const MiniAppHttpRequest *req, MiniAppHttpResponse *res, void *ctx);
    void *ctx;
} MiniAppMiddleware;

typedef struct {
    MiniAppMiddleware middlewares[MINI_APP_MAX_MIDDLEWARE];
    int count;
} MiniAppMiddlewareChain;

void mini_app_middleware_chain_init(MiniAppMiddlewareChain *chain);
int  mini_app_middleware_add(MiniAppMiddlewareChain *chain, const char *name,
                              bool (*pre)(const MiniAppHttpRequest *, MiniAppHttpResponse *, void *),
                              void (*post)(const MiniAppHttpRequest *, MiniAppHttpResponse *, void *),
                              void *ctx);
int  mini_app_middleware_run_pre(MiniAppMiddlewareChain *chain,
                                  const MiniAppHttpRequest *req, MiniAppHttpResponse *res);
void mini_app_middleware_run_post(MiniAppMiddlewareChain *chain,
                                   const MiniAppHttpRequest *req, MiniAppHttpResponse *res);
void mini_app_middleware_add_logging(MiniAppMiddlewareChain *chain);
void mini_app_middleware_add_cors(MiniAppMiddlewareChain *chain);
void mini_app_middleware_add_security_headers(MiniAppMiddlewareChain *chain);

/* Query params / helpers */
#define MINI_APP_MAX_QUERY_PARAMS 32
typedef struct {
    char key[128];
    char value[512];
} MiniAppQueryParam;

int mini_app_parse_query_string(const char *uri, MiniAppQueryParam *params, int max_params);
const char *mini_app_get_header(const MiniAppHttpRequest *req, const char *key);
bool mini_app_accepts(const MiniAppHttpRequest *req, const char *mime_type);

/* WebSocket */
bool mini_app_websocket_upgrade(const MiniAppHttpRequest *req, MiniAppHttpResponse *res);

/* SSE */
void mini_app_sse_begin(MiniAppHttpResponse *res);
void mini_app_sse_send_event(MiniAppHttpResponse *res, const char *event_type, const char *data);

/* Compression negotiation */
typedef enum {
    MINI_APP_COMPRESS_NONE    = 0,
    MINI_APP_COMPRESS_GZIP    = 1,
    MINI_APP_COMPRESS_DEFLATE = 2,
} MiniAppCompressAlgo;

MiniAppCompressAlgo mini_app_negotiate_encoding(const MiniAppHttpRequest *req);
const char *mini_app_compress_algo_str(MiniAppCompressAlgo algo);

#endif

