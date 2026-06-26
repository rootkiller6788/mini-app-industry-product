#include "package_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ================================================================
 * package_manager.c -- Package Dependency Manager Implementation
 *
 * L1: SemVer 2.0.0 parsing and comparison
 * L4: SemVer precedence rules (https://semver.org)
 * L5: Kahn's algorithm for topological sort (dependency resolution)
 * L6: Package manager with install, upgrade, conflict resolution
 * ================================================================ */

/* ---- L1: SemVer Parsing (spec 2.0.0) ---- */

SemVer semver_parse(const char *str) {
    SemVer v = {0, 0, 0, "", ""};
    if (!str) return v;
    char buf[PM_MAX_VERSION_LEN];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Parse MAJOR.MINOR.PATCH */
    char *dot1 = strchr(buf, '.');
    if (!dot1) return v;
    *dot1 = '\0';
    v.major = atoi(buf);

    char *dot2 = strchr(dot1 + 1, '.');
    if (!dot2) { *dot1 = '.'; return v; }
    *dot2 = '\0';
    v.minor = atoi(dot1 + 1);

    char *rest = dot2 + 1;
    char *dash = strchr(rest, '-');
    char *plus = strchr(rest, '+');
    if (dash) *dash = '\0';
    v.patch = atoi(rest);

    if (dash) {
        *dash = '-';
        char *prerelease = dash + 1;
        if (plus) *plus = '\0';
        strncpy(v.prerelease, prerelease, sizeof(v.prerelease) - 1);
        if (plus) *plus = '+';
    }
    if (plus) {
        strncpy(v.build, plus + 1, sizeof(v.build) - 1);
    }
    *dot1 = '.';
    if (dot2) *dot2 = '.';
    return v;
}

/* Compare two identifiers (numeric or alphanumeric)
 * Per spec: numeric identifiers compare numerically
 *           alphanumeric identifiers compare lexically */
static int compare_ident(const char *a, const char *b) {
    int na = 0, nb = 0;
    sscanf(a, "%d", &na);
    sscanf(b, "%d", &nb);
    /* Check if both are numeric */
    int anum = 1, bnum = 1;
    for (int i = 0; a[i]; i++) if (!isdigit((unsigned char)a[i])) anum = 0;
    for (int i = 0; b[i]; i++) if (!isdigit((unsigned char)b[i])) bnum = 0;
    if (anum && bnum) return (na > nb) - (na < nb);
    if (anum) return -1;  /* numeric < alphanumeric */
    if (bnum) return 1;
    return strcmp(a, b);
}

int semver_compare(SemVer a, SemVer b) {
    if (a.major != b.major) return (a.major > b.major) ? 1 : -1;
    if (a.minor != b.minor) return (a.minor > b.minor) ? 1 : -1;
    if (a.patch != b.patch) return (a.patch > b.patch) ? 1 : -1;
    /* Pre-release precedence */
    int a_has_pre = (a.prerelease[0] != '\0');
    int b_has_pre = (b.prerelease[0] != '\0');
    if (!a_has_pre && !b_has_pre) return 0;
    if (!a_has_pre && b_has_pre) return 1;
    if (a_has_pre && !b_has_pre) return -1;
    /* Compare pre-release dot-separated fields */
    char a_buf[32], b_buf[32];
    snprintf(a_buf, sizeof(a_buf), "%s", a.prerelease);
    snprintf(b_buf, sizeof(b_buf), "%s", b.prerelease);
    char *at = strtok(a_buf, ".");
    char *bt = strtok(b_buf, ".");
    while (at || bt) {
        if (!at) return -1;
        if (!bt) return 1;
        int cmp = compare_ident(at, bt);
        if (cmp != 0) return cmp;
        at = strtok(NULL, ".");
        bt = strtok(NULL, ".");
    }
    return 0;
}

/* Check if version satisfies a constraint */
int semver_satisfies(SemVer version, SemVer constraint_ver, DepConstraint c) {
    int cmp = semver_compare(version, constraint_ver);
    switch (c) {
        case DEP_EQUAL:         return cmp == 0;
        case DEP_GREATER_THAN:  return cmp > 0;
        case DEP_LESS_THAN:     return cmp < 0;
        case DEP_GTE:           return cmp >= 0;
        case DEP_LTE:           return cmp <= 0;
        case DEP_CARET:  /* ^1.2.3 => >=1.2.3 <2.0.0 */
            if (constraint_ver.major == 0 && constraint_ver.minor == 0)
                return cmp == 0;
            if (constraint_ver.major == 0)
                return version.major == 0 && version.minor == constraint_ver.minor && cmp >= 0;
            return version.major == constraint_ver.major && cmp >= 0;
        case DEP_TILDE:  /* ~1.2.3 => >=1.2.3 <1.3.0 */
            return version.major == constraint_ver.major &&
                   version.minor == constraint_ver.minor && cmp >= 0;
        case DEP_ANY: return 1;
        default: return 0;
    }
}

char* semver_format(SemVer v, char *buf, int buf_sz) {
    if (!buf || buf_sz <= 0) return NULL;
    int off = snprintf(buf, buf_sz, "%d.%d.%d", v.major, v.minor, v.patch);
    if (v.prerelease[0]) off += snprintf(buf + off, buf_sz - off, "-%s", v.prerelease);
    if (v.build[0])       off += snprintf(buf + off, buf_sz - off, "+%s", v.build);
    return buf;
}

bool semver_is_valid(const char *str) {
    if (!str) return false;
    SemVer v = semver_parse(str);
    return v.major >= 0 && v.minor >= 0 && v.patch >= 0;
}

/* ---- L5: Topological Sort (Kahn's Algorithm, O(V+E)) ---- */
/* Theorem: A directed graph has a topological ordering iff it is acyclic.
 * Kahn's algorithm detects cycles: if result has fewer nodes than the graph,
 * a cycle exists.
 */

int* pm_topological_sort(Package *pkgs[], int n_deps, int deps[][PM_MAX_DEPS_PER_PKG],
                          int dep_counts[], int *out_count) {
    if (!pkgs || !deps || !dep_counts || !out_count || n_deps <= 0) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    int *indegree = (int*)calloc((size_t)n_deps, sizeof(int));
    int *result   = (int*)malloc((size_t)n_deps * sizeof(int));
    if (!indegree || !result) { free(indegree); free(result); *out_count = 0; return NULL; }

    /* Compute in-degrees */
    for (int i = 0; i < n_deps; i++)
        indegree[i] = dep_counts[i];

    /* Queue for nodes with indegree 0 */
    int *queue = (int*)malloc((size_t)n_deps * sizeof(int));
    if (!queue) { free(indegree); free(result); *out_count = 0; return NULL; }
    int qh = 0, qt = 0;
    for (int i = 0; i < n_deps; i++)
        if (indegree[i] == 0) queue[qt++] = i;

    int idx = 0;
    while (qh < qt) {
        int u = queue[qh++];
        result[idx++] = u;
        for (int v = 0; v < n_deps; v++) {
            if (v == u) continue;
            /* Check if u is a dependency of v */
            for (int d = 0; d < dep_counts[v]; d++) {
                if (deps[v][d] == u) {
                    indegree[v]--;
                    if (indegree[v] == 0) queue[qt++] = v;
                }
            }
        }
    }
    free(queue);
    free(indegree);
    *out_count = idx;
    if (idx < n_deps) {
        /* Cycle detected: remaining nodes have circular dependencies */
        free(result);
        *out_count = 0;
        return NULL;
    }
    return result;
}

/* ---- L3: Registry operations ---- */

PackageRegistry* pm_registry_create(int capacity) {
    PackageRegistry *reg = (PackageRegistry*)calloc(1, sizeof(PackageRegistry));
    if (!reg) return NULL;
    reg->capacity = capacity > 0 ? capacity : PM_MAX_REGISTRY_PKGS;
    return reg;
}

void pm_registry_destroy(PackageRegistry *reg) {
    if (reg) free(reg);
}

int pm_registry_add(PackageRegistry *reg, const Package *pkg) {
    if (!reg || !pkg || reg->count >= PM_MAX_PACKAGES) return -1;
    /* Check for duplicates */
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->packages[i].name, pkg->name) == 0 &&
            semver_compare(reg->packages[i].version, pkg->version) == 0)
            return -1;
    }
    reg->packages[reg->count] = *pkg;
    reg->count++;
    return 0;
}

Package* pm_registry_find(PackageRegistry *reg, const char *name) {
    if (!reg || !name) return NULL;
    SemVer best = {0, 0, 0, "", ""};
    Package *found = NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->packages[i].name, name) == 0) {
            if (!found || semver_compare(reg->packages[i].version, best) > 0) {
                best = reg->packages[i].version;
                found = &reg->packages[i];
            }
        }
    }
    return found;
}

Package* pm_registry_find_version(PackageRegistry *reg, const char *name, SemVer v) {
    if (!reg || !name) return NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->packages[i].name, name) == 0 &&
            semver_compare(reg->packages[i].version, v) == 0)
            return &reg->packages[i];
    }
    return NULL;
}

int pm_registry_remove(PackageRegistry *reg, const char *name) {
    if (!reg || !name) return -1;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->packages[i].name, name) == 0) {
            for (int j = i; j < reg->count - 1; j++)
                reg->packages[j] = reg->packages[j + 1];
            reg->count--;
            return 0;
        }
    }
    return -1;
}

/* ---- L6: Package Install / Uninstall / Upgrade ---- */

int pm_install(PackageRegistry *reg, const char *name, SemVer version) {
    if (!reg || !name) return -1;
    Package *pkg = pm_registry_find_version(reg, name, version);
    if (!pkg) return -1;
    if (pkg->installed && semver_compare(pkg->installed_version, version) == 0)
        return 0;  /* already installed */
    pkg->installed = true;
    pkg->installed_version = version;
    return 0;
}

int pm_uninstall(PackageRegistry *reg, const char *name) {
    if (!reg || !name) return -1;
    Package *pkg = pm_registry_find(reg, name);
    if (!pkg || !pkg->installed) return -1;
    pkg->installed = false;
    return 0;
}

int pm_upgrade(PackageRegistry *reg, const char *name, SemVer target) {
    if (!reg || !name) return -1;
    Package *pkg = pm_registry_find(reg, name);
    if (!pkg || !pkg->installed) return -1;
    if (semver_compare(target, pkg->installed_version) <= 0) return -1;
    Package *tgt = pm_registry_find_version(reg, name, target);
    if (!tgt) return -1;
    tgt->installed = true;
    tgt->installed_version = target;
    pkg->installed = false;
    return 0;
}

int pm_list_installed(const PackageRegistry *reg, Package *out, int max_out) {
    if (!reg || !out) return 0;
    int cnt = 0;
    for (int i = 0; i < reg->count && cnt < max_out; i++) {
        if (reg->packages[i].installed) out[cnt++] = reg->packages[i];
    }
    return cnt;
}

bool pm_check_conflicts(PackageRegistry *reg, const char *name, SemVer v) {
    if (!reg || !name) return true;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->packages[i].name, name) == 0) continue;
        if (!reg->packages[i].installed) continue;
        /* Check if this package has a conflicting dep on name */
        for (int d = 0; d < reg->packages[i].dep_count; d++) {
            if (strcmp(reg->packages[i].deps[d].name, name) == 0) {
                if (!semver_satisfies(v, reg->packages[i].deps[d].version,
                                      reg->packages[i].deps[d].constraint))
                    return true;
            }
        }
    }
    return false;
}

/* ---- L5: Dependency Resolution ---- */

ResolveResult pm_resolve_deps(PackageRegistry *reg, const char *pkg_name, SemVer want) {
    ResolveResult res;
    memset(&res, 0, sizeof(res));
    res.success = false;
    if (!reg || !pkg_name) return res;

    Package *root = pm_registry_find_version(reg, pkg_name, want);
    if (!root) {
        snprintf(res.error_msg, sizeof(res.error_msg), "Package not found: %s", pkg_name);
        return res;
    }

    /* BFS-based resolution with visited set */
    int visited[PM_MAX_PACKAGES] = {0};
    int queue[PM_MAX_PACKAGES];
    int qh = 0, qt = 0;

    /* Find root index */
    int root_idx = -1;
    for (int i = 0; i < reg->count; i++) {
        if (&reg->packages[i] == root) { root_idx = i; break; }
    }
    if (root_idx < 0) { snprintf(res.error_msg, sizeof(res.error_msg), "Internal error"); return res; }

    queue[qt++] = root_idx;
    visited[root_idx] = 1;

    while (qh < qt && res.resolved_count < PM_MAX_PACKAGES) {
        int ci = queue[qh++];
        res.resolved[res.resolved_count++] = &reg->packages[ci];
        Package *cp = &reg->packages[ci];

        for (int d = 0; d < cp->dep_count; d++) {
            /* Find best matching version for this dep */
            Package *best = NULL;
            for (int j = 0; j < reg->count; j++) {
                if (strcmp(reg->packages[j].name, cp->deps[d].name) == 0) {
                    if (semver_satisfies(reg->packages[j].version, cp->deps[d].version,
                                         cp->deps[d].constraint)) {
                        if (!best || semver_compare(reg->packages[j].version, best->version) > 0)
                            best = &reg->packages[j];
                    }
                }
            }
            if (!best) {
                snprintf(res.error_msg, sizeof(res.error_msg),
                         "Unresolved dependency: %s for %s",
                         cp->deps[d].name, cp->name);
                res.conflict_count = 1;
                strncpy(res.conflicts[0], cp->deps[d].name, PM_MAX_NAME_LEN - 1);
                return res;
            }
            int bi = -1;
            for (int j = 0; j < reg->count; j++)
                if (&reg->packages[j] == best) { bi = j; break; }
            if (bi >= 0 && !visited[bi]) {
                visited[bi] = 1;
                queue[qt++] = bi;
            }
        }
    }
    res.success = true;
    return res;
}

/* ---- L4: Version range operations ---- */

SemVer semver_latest(const Package *versions[], int count) {
    SemVer best = {0, 0, 0, "", ""};
    if (!versions || count <= 0) return best;
    best = versions[0]->version;
    for (int i = 1; i < count; i++)
        if (semver_compare(versions[i]->version, best) > 0)
            best = versions[i]->version;
    return best;
}

SemVer semver_next_major(SemVer v) {
    return (SemVer){v.major + 1, 0, 0, "", ""};
}

SemVer semver_next_minor(SemVer v) {
    return (SemVer){v.major, v.minor + 1, 0, "", ""};
}

SemVer semver_next_patch(SemVer v) {
    return (SemVer){v.major, v.minor, v.patch + 1, "", ""};
}

int semver_range_satisfies(SemVer v, const char *range_str) {
    if (!range_str || !range_str[0]) return 1;
    char buf[64];
    strncpy(buf, range_str, sizeof(buf) - 1);
    buf[63] = '\0';

    /* Handle caret: ^1.2.3 */
    if (buf[0] == '^') {
        SemVer cv = semver_parse(buf + 1);
        return semver_satisfies(v, cv, DEP_CARET);
    }
    /* Handle tilde: ~1.2.3 */
    if (buf[0] == '~') {
        SemVer cv = semver_parse(buf + 1);
        return semver_satisfies(v, cv, DEP_TILDE);
    }
    /* Handle >=, <=, >, <, = */
    if (strncmp(buf, ">=", 2) == 0) {
        SemVer cv = semver_parse(buf + 2);
        return semver_compare(v, cv) >= 0;
    }
    if (strncmp(buf, "<=", 2) == 0) {
        SemVer cv = semver_parse(buf + 2);
        return semver_compare(v, cv) <= 0;
    }
    if (buf[0] == '>') {
        SemVer cv = semver_parse(buf + 1);
        return semver_compare(v, cv) > 0;
    }
    if (buf[0] == '<') {
        SemVer cv = semver_parse(buf + 1);
        return semver_compare(v, cv) < 0;
    }
    if (buf[0] == '=') {
        SemVer cv = semver_parse(buf + 1);
        return semver_compare(v, cv) == 0;
    }
    /* Exact version */
    SemVer cv = semver_parse(buf);
    return semver_compare(v, cv) == 0;
}

int pm_find_updates(const PackageRegistry *reg, Package *out, int max_out) {
    if (!reg || !out) return 0;
    int cnt = 0;
    for (int i = 0; i < reg->count && cnt < max_out; i++) {
        if (!reg->packages[i].installed) continue;
        Package *latest = pm_registry_find((PackageRegistry*)reg, reg->packages[i].name);
        if (latest && semver_compare(latest->version, reg->packages[i].installed_version) > 0)
            out[cnt++] = *latest;
    }
    return cnt;
}

/* ---- Utility ---- */

const char* dep_constraint_str(DepConstraint c) {
    switch (c) {
        case DEP_EQUAL:        return "==";
        case DEP_GREATER_THAN: return ">";
        case DEP_LESS_THAN:    return "<";
        case DEP_GTE:          return ">=";
        case DEP_LTE:          return "<=";
        case DEP_CARET:        return "^";
        case DEP_TILDE:        return "~";
        case DEP_ANY:          return "*";
        default: return "?";
    }
}

int pm_validate_package(const Package *pkg) {
    if (!pkg) return 0;
    if (!pkg->name[0]) return 0;
    if (pkg->version.major < 0) return 0;
    return 1;
}

void pm_format_tree(const PackageRegistry *reg, const char *root, char *buf, int buf_sz) {
    if (!reg || !root || !buf) return;
    int off = snprintf(buf, buf_sz, "Package tree for: %s\n", root);
    Package *rp = pm_registry_find((PackageRegistry*)reg, root);
    if (!rp) { snprintf(buf + off, buf_sz - off, "  (not found)\n"); return; }
    char ver[32];
    off += snprintf(buf + off, buf_sz - off, "  [%s] %s\n",
                    semver_format(rp->installed ? rp->installed_version : rp->version, ver, sizeof(ver)),
                    rp->name);
    for (int d = 0; d < rp->dep_count && off < buf_sz; d++) {
        off += snprintf(buf + off, buf_sz - off, "    |-- %s %s\n",
                        rp->deps[d].name, dep_constraint_str(rp->deps[d].constraint));
    }
}
