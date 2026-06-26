# mini-internet-app — 互联网应用 (Internet Application Framework)

纯 C99 实现的微型 Web 应用框架，涵盖 HTTP 服务器、URL 路由、会话认证、JSON API、模板引擎、限流、缓存等互联网应用核心技术。

## Module Status: COMPLETE ✅

- **L1-L6**: Complete
- **L7**: Complete (3 applications: REST API, 模板渲染, WebSocket)
- **L8**: Partial (分布式限流, 中间件管道已实现)
- **L9**: Partial (仅文档)

## 构建

```bash
make          # 编译
make test     # 运行测试 (21 tests, 0 failures)
make examples # 编译示例
make clean
```

## 模块

| 模块 | 头文件 | 功能 |
|------|--------|------|
| Web 服务器 | web_server.h | HTTP/1.1 解析, 响应构建, 路由分发, 中间件, WebSocket, SSE |
| URL 路由 | url_router.h | Trie 路由匹配, 参数捕获, URL 编解码 |
| 会话认证 | session_auth.h | 会话管理, 密码哈希, JWT Token |
| JSON API | json_api.h | JSON 解析/序列化, REST API 构建 |
| 限流器 | rate_limiter.h | 令牌桶, 滑动窗口, 分布式一致性哈希 |
| 模板引擎 | template_engine.h | 变量替换, 条件/循环, HTML 转义, 模板继承 |
| 内容缓存 | content_cache.h | LRU 缓存, ETag, 过期策略 |
