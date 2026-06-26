# mini-dev-tools-platform -- Developer Tools Platform

Comprehensive dev tools framework in C99: structured logging, config management,
CLI parsing, profiling, feature flags, code review, deployment orchestration,
package management, and test framework.

## Module Status: COMPLETE

- L1-L6: Complete
- L7: Complete (3 applications)
- L8: Partial (2+ advanced topics)
- L9: Partial (documented, not implemented)

## Modules (9 headers, 9 sources, 3348 lines)

| Module | Description | Key Feature |
|--------|-------------|-------------|
| **logger** | Structured logging | Multi-sink: console/file/ringbuf/JSON/UDP, colorized output |
| **config_mgr** | Configuration management | Hot-reload with env vars, schema validation, JSON export |
| **cli_parser** | CLI argument parsing | GNU-style with subcommands, typed flags, help generation |
| **profiler** | Performance profiling | CPU/memory profiling with flamegraph, checkpoint, merge |
| **feature_flags** | Feature flag system | Boolean/percentage/targeted/scheduled, hash-bucketing |
| **code_review** | Code review pipeline | Review management with CI checks, threading, approval flow |
| **deploy_manager** | Deployment orchestration | Multi-step pipeline with auto-rollback |
| **package_manager** | Package dependency manager | SemVer 2.0.0, topological sort (Kahn algorithm) |
| **test_runner** | xUnit test framework | Fixture-based, JUnit XML output, tagged execution |

## Quick Start

```sh
make test      # Run all tests (21/21 pass)
make check     # Syntax check all sources
make examples  # Build and run demo suite
make clean     # Clean build artifacts
```

## Core Definitions (L1)

- 18 struct/enum types covering logging, config, CLI, profiling, feature flags,
  code review, deployment, package management, and testing
- 50+ public API functions with full C99 type annotations

## Core Theorems (L4)

| Theorem | Application |
|---------|------------|
| **Semantic Versioning 2.0.0** (Preston-Werner, 2011) | Package version comparison with precedence rules |
| **Kahn Algorithm** (Topological Sorting, CACM 1962) | O(V+E) dependency resolution with cycle detection |
| **xUnit Test Patterns** (Meszaros, 2007) | Fixture lifecycle with setup/teardown hooks |
| **djb2 Hash** (Bernstein) | Deterministic feature flag percentage bucketing |

### SemVer Precedence Formula
```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
1. Compare MAJOR, MINOR, PATCH numerically
2. A pre-release version has LOWER precedence than a normal version
3. Pre-release identifiers compared alphanumerically as dot-separated fields
4. Build metadata is IGNORED in precedence
```

## Core Algorithms (L5)

| Algorithm | Complexity | Implementation |
|-----------|-----------|---------------|
| Kahn Topological Sort | O(V + E) | package_manager.c |
| BFS Dependency Resolution | O(V + E) | package_manager.c |
| djb2 Hash Bucketing | O(n) | feature_flags.c |
| SemVer Comparison | O(k) for k fields | package_manager.c |

## Canonical Problems (L6)

- **CI/CD Pipeline Simulation**: Full pipeline from code review to deployment
- **Trunk-Based Development**: Feature flags for gradual rollouts
- **Package Dependency Resolution**: npm-style dependency graphs
- **Multi-Sink Structured Logging**: Console, JSON, ring buffer, UDP sinks
- **Configuration Hot-Reload**: Environment overlays with version tracking
- **Performance Profiling**: Flamegraph generation with memory tracking
- **Deployment Orchestration**: Multi-step pipeline with auto-rollback

See `examples/example_demo.c` for end-to-end demonstration.

## Nine-School Curriculum Mapping

| School | Key Courses | Module Alignment |
|--------|------------|-----------------|
| **MIT** | 6.004, 6.824, 6.858 | Config management, deployment, dependency verification |
| **Stanford** | CS 144, CS 245, CS 229 | Networked logging, query profiling, A/B testing |
| **Berkeley** | CS 162, CS 186, CS 267 | Resource monitoring, transaction logging, HPC profiling |
| **CMU** | 15-410, 15-445, 15-418 | System instrumentation, IO profiling, bottleneck analysis |
| **ETH** | 263-0006, 263-3501 | Performance counters, parallel profiling |
| **Cambridge** | OS, Concurrent Systems | System tools, pipeline execution |
| **Tsinghua** | OS, Compilers | System utilities, build dependency management |
| **Georgia Tech** | CS 6210, CS 6290 | System profiling, performance analysis |

## Documentation

- [Knowledge Graph](docs/knowledge-graph.md) -- L1-L9 coverage
- [Coverage Report](docs/coverage-report.md) -- Line counts and test results
- [Course Alignment](docs/course-alignment.md) -- Nine-school curriculum mapping

## License
MIT