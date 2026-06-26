# Course Alignment — mini-dev-tools-platform

## Nine-School Curriculum Mapping

### MIT
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| 6.004 Computation Structures | System layers | Profiler, Logger (observability) |
| 6.824 Distributed Systems | Config management, deployment | ConfigMgr (hot-reload), DeployManager (canary) |
| 6.858 Computer Security | Dependency verification | PackageManager (checksum, version pinning) |

### Stanford
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| CS 144 Networking | Networked logging | Logger (UDP sink) |
| CS 245 Database | Query optimization profiling | Profiler (query zone tracking) |
| CS 229 Machine Learning | A/B testing infrastructure | FeatureFlags (percentage rollout) |

### Berkeley
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| CS 162 Operating Systems | Resource monitoring | Profiler (CPU/memory) |
| CS 186 Database Systems | Transaction logging | Logger (structured, ring buffer) |
| CS 267 HPC | Performance profiling | Profiler (flamegraph generation) |

### CMU
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| 15-410 Operating Systems | System instrumentation | Profiler (zone tracking) |
| 15-445 Database Systems | Query plan profiling | Profiler (IO zones) |
| 15-721 Advanced DB | Storage profiling | Profiler (allocation tracking) |
| 15-418 Parallel Computing | Bottleneck analysis | Profiler (zone comparison) |

### UT Austin
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| CS 380D Distributed Systems | Service deployment | DeployManager (multi-step) |
| CS 395T Systems ML | Experiment tracking | FeatureFlags (targeted rollout) |

### ETH Zurich
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| 263-0006 Computer Architecture | Performance counters | Profiler (CPU zones) |
| 263-3501 Parallel Programming | Profiling tools | Profiler (flamegraph) |

### Cambridge
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| Part II: Operating Systems | System tools | Logger, Profiler |
| Concurrent Systems | Pipeline execution | DeployManager (step orchestration) |
| Compiler Construction | Build tooling | PackageManager (dependency resolution) |

### Tsinghua (清华)
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| 操作系统 | System utilities | Logger, ConfigMgr |
| 编译原理 | Build dependency management | PackageManager (topological sort) |
| 计算机网络 | Networked logging | Logger (UDP sink) |

### Georgia Tech
| Course | Topic | Module Coverage |
|--------|-------|----------------|
| CS 6210 Advanced OS | System profiling | Profiler |
| CS 6290 HPCA | Performance analysis | Profiler (CPU utilization) |
| CS 7641 Machine Learning | A/B testing | FeatureFlags |

## Dependency Tree (Course Prerequisites)

```
Data Structures (L1-L3)
    ├── Logger (structured logging)
    ├── ConfigMgr (hash tables, string parsing)
    │
Algorithms (L4-L5)
    ├── PackageManager (topological sort, BFS)
    ├── FeatureFlags (hash bucketing)
    │
Software Engineering (L6)
    ├── CodeReview (workflow patterns)
    ├── DeployManager (pipeline orchestration)
    ├── TestRunner (xUnit patterns)
    │
Applications (L7-L9)
    ├── CI/CD Pipeline (integration of all modules)
    ├── Progressive Delivery (feature flags + deployment)
    └── Observability Platform (logging + profiling)
```