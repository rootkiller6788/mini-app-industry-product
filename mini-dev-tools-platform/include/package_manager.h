#ifndef PACKAGE_MANAGER_H
#define PACKAGE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * package_manager.h -- Package Dependency Manager
 *
 * L1: SemVer (Semantic Versioning 2.0.0) parsing and comparison
 * L4: SemVer spec (https://semver.org) with precedence rules
 * L5: Topological sort for dependency resolution (Kahn's algorithm, BFS-based)
 *     Cycle detection via Kahn's in-degree counting
 * L6: Package manager with install, upgrade, conflict resolution
 *
 * SemVer precedence: MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
 *   1. Compare MAJOR, MINOR, PATCH numerically
 *   2. Pre-release has lower precedence
 *   3. Pre-release fields compared alphanumerically
 * Build metadata is ignored in precedence
 */

#define PM_MAX_PACKAGES      256
#define PM_MAX_DEPS_PER_PKG   32
#define PM_MAX_NAME_LEN        64
#define PM_MAX_VERSION_LEN     32
#define PM_MAX_URL_LEN        512
#define PM_MAX_REGISTRY_PKGS  1024

/* SemVer struct per spec 2.0.0 */
typedef struct {
    int major;
    int minor;
    int patch;
    char prerelease[32];
    char build[32];
} SemVer;

typedef enum {
    DEP_EQUAL,
    DEP_GREATER_THAN,
    DEP_LESS_THAN,
    DEP_GTE,
    DEP_LTE,
    DEP_CARET,
    DEP_TILDE,
    DEP_ANY
} DepConstraint;

typedef struct {
    char name[PM_MAX_NAME_LEN];
    DepConstraint constraint;
    SemVer version;
} PackageDep;

typedef struct {
    char name[PM_MAX_NAME_LEN];
    SemVer version;
    char description[256];
    char url[PM_MAX_URL_LEN];
    PackageDep deps[PM_MAX_DEPS_PER_PKG];
    int dep_count;
    bool installed;
    SemVer installed_version;
    uint64_t size_bytes;
    char checksum[65];  /* SHA-256 hex */
} Package;

typedef struct {
    Package packages[PM_MAX_PACKAGES];
    int count;
    int capacity;
} PackageRegistry;

typedef struct {
    Package *resolved[PM_MAX_PACKAGES];
    int resolved_count;
    char conflicts[PM_MAX_PACKAGES][PM_MAX_NAME_LEN];
    int conflict_count;
    char error_msg[512];
    bool success;
} ResolveResult;

/* L1: SemVer API */
SemVer semver_parse(const char *str);
int    semver_compare(SemVer a, SemVer b);
int    semver_satisfies(SemVer version, SemVer constraint_ver, DepConstraint c);
char*  semver_format(SemVer v, char *buf, int buf_sz);
bool   semver_is_valid(const char *str);

/* L5: Dependency Resolution (Kahn's Algorithm) */
ResolveResult pm_resolve_deps(PackageRegistry *reg, const char *pkg_name, SemVer want);
int*   pm_topological_sort(Package *pkgs[], int n_deps, int deps[][PM_MAX_DEPS_PER_PKG],
                            int dep_counts[], int *out_count);

/* L3: Registry operations */
PackageRegistry* pm_registry_create(int capacity);
void             pm_registry_destroy(PackageRegistry *reg);
int              pm_registry_add(PackageRegistry *reg, const Package *pkg);
Package*         pm_registry_find(PackageRegistry *reg, const char *name);
Package*         pm_registry_find_version(PackageRegistry *reg, const char *name, SemVer v);
int              pm_registry_remove(PackageRegistry *reg, const char *name);

/* L6: Package management operations */
int  pm_install(PackageRegistry *reg, const char *name, SemVer version);
int  pm_uninstall(PackageRegistry *reg, const char *name);
int  pm_upgrade(PackageRegistry *reg, const char *name, SemVer target);
int  pm_list_installed(const PackageRegistry *reg, Package *out, int max_out);
bool pm_check_conflicts(PackageRegistry *reg, const char *name, SemVer v);
int  pm_find_updates(const PackageRegistry *reg, Package *out, int max_out);

/* L4: Version range operations */
SemVer semver_latest(const Package *versions[], int count);
SemVer semver_next_major(SemVer v);
SemVer semver_next_minor(SemVer v);
SemVer semver_next_patch(SemVer v);
int    semver_range_satisfies(SemVer v, const char *range_str);

/* Utility */
const char* dep_constraint_str(DepConstraint c);
int  pm_validate_package(const Package *pkg);
void pm_format_tree(const PackageRegistry *reg, const char *root, char *buf, int buf_sz);

#endif
