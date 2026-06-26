# Knowledge Graph — mini-dev-tools-platform

## L1: Core Definitions

| Concept | Type | Location |
|---------|------|----------|
| LogLevel (TRACE..FATAL) | enum | logger.h:13-20 |
| SinkType (CONSOLE..JSON) | enum | logger.h:22-29 |
| LogEntry, LogSink, Logger | struct | logger.h:31-61 |
| ConfigType (STRING..JSON) | enum | config_mgr.h:14-20 |
| ConfigSource (FILE..CMDLINE) | enum | config_mgr.h:22-29 |
| ConfigEntry, ConfigManager | struct | config_mgr.h:31-51 |
| FlagType (BOOL..COUNT) | enum | cli_parser.h:13-19 |
| Flag, CLICommand, CLIParser | struct | cli_parser.h:21-59 |
| ProfileType (CPU..IO) | enum | profiler.h:13-18 |
| ProfileZone, MemoryStats, Profiler | struct | profiler.h:20-53 |
| FeatureFlagType (BOOLEAN..SCHEDULED) | enum | feature_flags.h:16-21 |
| FeatureFlagState (OFF..REMOVED) | enum | feature_flags.h:23-30 |
| FeatureFlag, FeatureFlagSystem | struct | feature_flags.h:32-68 |
| ReviewStatus, CheckStatus | enum | code_review.h:15-28 |
| ReviewComment, CodeCheck, CodeReview | struct | code_review.h:30-66 |
| DeployEnvironment, DeployStatus, DeployStepType | enum | deploy_manager.h:14-37 |
| DeployStep, DeployManager | struct | deploy_manager.h:39-68 |
| SemVer, DepConstraint, PackageDep | struct/enum | package_manager.h:31-53 |
| Package, PackageRegistry, ResolveResult | struct | package_manager.h:55-81 |
| TrResult, TrTest, TrTestCase, TrSuite, TrRunner | struct/enum | test_runner.h:25-79 |

**Status: COMPLETE** — All core definitions have C struct/typedef and API declarations.

## L2: Core Concepts

| Concept | Implementation | File |
|---------|---------------|------|
| Structured Logging | Multi-sink logger with JSON, ring buffer, console | logger.c |
| Configuration Management | Hot-reload with env vars, schema validation | config_mgr.c |
| CLI Parsing | GNU-style with subcommands, typed flags | cli_parser.c |
| Profiling | CPU/memory profiling with flamegraph | profiler.c |
| Feature Flags | Boolean/percentage/targeted/scheduled flags | feature_flags.c |
| Code Review | Review management with CI checks | code_review.c |
| Deployment Orchestration | Multi-step pipeline with auto-rollback | deploy_manager.c |
| Package Management | SemVer parsing, dependency resolution | package_manager.c |
| Test Framework | xUnit-style runner with JUnit XML output | test_runner.c |

**Status: COMPLETE** — All core concepts have corresponding implementation modules.

## L3: Engineering Structures

| Structure | Description | File |
|-----------|-------------|------|
| Log Sink Architecture | SinkType enum + polymorphic dispatch via write_to_sink() | logger.c |
| Config Merge Pattern | Overlay merge with source tracking | config_mgr.c |
| CLI Command Tree | Recursive CLICommand with subcommands | cli_parser.c |
| Profiler Zone Stack | Zone begin/end with call counting | profiler.c |
| Feature Flag Evaluation | Hash-based percentage bucketing + rule matching | feature_flags.c |
| Review Pipeline | Comment threading + CI check aggregation | code_review.c |
| Deploy State Machine | IDLE→IN_PROGRESS→SUCCEEDED/FAILED→ROLLING_BACK | deploy_manager.c |
| Dependency Graph | Adjacency list representation for packages | package_manager.c |
| Test Fixture Lifecycle | setup→test→teardown with suite organization | test_runner.c |

**Status: COMPLETE** — All engineering structures have complete data types and operations.

## L4: Standards and Theorems

| Standard/Theorem | Verification | Location |
|-----------------|-------------|----------|
| Semantic Versioning 2.0.0 (semver.org) | semver_parse/compare with precedence rules | package_manager.c:18-100 |
| Kahn's Algorithm Correctness (1962) | Cycle detection via indegree counting | package_manager.c:145-191 |
| CAP Theorem (Brewer, 2000) | Documented in config management (eventual consistency via hot-reload) | docs/ |
| xUnit Test Patterns (Meszaros, 2007) | Fixture-based test runner with setup/teardown | test_runner.c |
| Hash-based Bucketing (djb2) | Deterministic percentage rollout | feature_flags.c:133-136 |

**L4 Status: COMPLETE** — Core theorems have code verification (SemVer, Kahn's) and documentation.

## L5: Algorithms and Methods

| Algorithm | Complexity | Implementation | File |
|-----------|-----------|---------------|------|
| Kahn's Topological Sort | O(V+E) | Indegree counting + BFS queue | package_manager.c:145-191 |
| djb2 Hash | O(n) | Hash-based feature flag bucketing | feature_flags.c:133 |
| SemVer Comparison | O(n) | Dot-separated field comparison | package_manager.c:60-100 |
| BFS Dependency Resolution | O(V+E) | Queue-based traversal with cycle detection | package_manager.c:319-384 |
| Bubble Sort (zone ranking) | O(n²) | Top-N profiler zones | profiler.c:203-222 |

**L5 Status: COMPLETE** — Each algorithm has at least one complete implementation.

## L6: Canonical Engineering Problems

| Problem | Solution | Example |
|---------|----------|---------|
| Structured Logging | Multi-sink logger with rotation | example_demo.c:28-50 |
| Configuration Hot-Reload | env/file/etcd sources with merge | example_demo.c:52-89 |
| Feature Flag System | Boolean/percentage/targeted/scheduled | example_demo.c:91-131 |
| Profiler with Flamegraph | CPU/memory profiling, allocation tracking | example_demo.c:133-169 |
| Code Review Pipeline | Comment threading, CI checks, merge gates | example_demo.c:171-201 |
| CI/CD Deployment | Multi-step pipeline with auto-rollback | example_demo.c:203-233 |
| Package Dependency Resolution | SemVer + topological sort | example_demo.c:235-283 |

**L6 Status: COMPLETE** — All canonical problems solved in examples/example_demo.c.

## L7: Applications

| Application | Description | Location |
|------------|-------------|----------|
| CI/CD Pipeline Simulation | Full pipeline: review→test→deploy→monitor | examples/example_demo.c |
| Trunk-Based Development | Feature flags for gradual rollout | examples/example_demo.c:91-131 |
| JUnit XML Report Generation | CI-friendly test output | test_runner.c:150-179 |

**L7 Status: COMPLETE** (3 applications) — Meets Partial+ requirement.

## L8: Advanced Topics

| Topic | Description | Location |
|-------|-------------|----------|
| Topological Dependency Resolution | Kahn's algorithm for package dependency graphs | package_manager.c:145-191 |
| SemVer Precedence Rules | Full spec 2.0.0 with pre-release ordering | package_manager.c:74-100 |

**L8 Status: PARTIAL** (2+ advanced topics) — Meets Partial+ requirement.

## L9: Industry Frontiers

| Topic | Description | Status |
|-------|-------------|--------|
| Modern CI/CD Patterns | Progressive delivery, canary deploys, feature flags | Documented |
| Package Management Evolution | npm-style dependency resolution, lock files | Documented |
| Observability | Structured logging → OpenTelemetry alignment | Documented |

**L9 Status: PARTIAL** — Documented, not all implemented. Meets requirement.