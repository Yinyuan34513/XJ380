#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "httpclient.h"

static const int XJAPT_MAX_PACKAGES = 80000;
static const int XJAPT_MAX_PLAN     = 512;
static const int XJAPT_LINE_MAX     = 2048;
static const char *XJAPT_CACHE_DIR = "/var/cache/xjapt";
static const char *XJAPT_DPKG_ADMINDIR = "/tmp/xjapt/dpkg";
static const char *XJAPT_DEFAULT_PACKAGES_URL =
    "http://mirrors.tuna.tsinghua.edu.cn/debian/dists/trixie/main/binary-amd64/";
static const char *XJAPT_DEFAULT_FILE_BASE_URL = "http://mirrors.tuna.tsinghua.edu.cn/debian/";

static void xjapt_serial(const char *message)
{
    xapi_OutputSerial((char *)message);
}

static void xjapt_serialf(const char *fmt, ...)
{
    char buffer[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    xjapt_serial(buffer);
}

struct Package
{
    char *name;
    char *filename;
    char *depends;
    int visit_state;
};

struct Repo
{
    char *index_url;
    char *file_base_url;
    Package packages[XJAPT_MAX_PACKAGES];
    int package_count;
};

static Package *find_package(Repo *repo, const char *name);

struct Plan
{
    Package *items[XJAPT_MAX_PLAN];
    int count;
};

struct Wanted
{
    char *name;
};

struct UrlParts
{
    char *host;
    char *path;
    uint16_t port;
};

struct DownloadState
{
    int fd;
    bool saw_header_end;
    int header_match;
    int status_code;
    char status_line[96];
    int status_len;
    bool status_line_done;
    char header_line[160];
    int header_line_len;
    bool have_content_length;
    size_t content_length;
    size_t body_bytes;
    size_t next_progress;
};

static void usage()
{
    fprintf(stderr, "usage: xjapt <package>\n");
    fprintf(stderr, "       xjapt <packages-dir-url> <package> [file-base-url]\n");
    fprintf(stderr, "default: Debian 13 trixie amd64 main from deb.debian.org\n");
    fprintf(stderr, "example: xjapt http://host/debian/dists/bookworm/main/binary-amd64/ lolcat http://host/debian/\n");
}

static char *trim_in_place_no_strlen(char *s)
{
    if (s == NULL) return s;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char *write = s;
    char *last_non_space = s - 1;
    for (char *read = s; *read != '\0'; read++) {
        *write = *read;
        if (*write != ' ' && *write != '\t' && *write != '\r' && *write != '\n') {
            last_non_space = write;
        }
        write++;
    }
    last_non_space[1] = '\0';
    return s;
}

static bool starts_with(const char *s, const char *prefix)
{
    return s != NULL && prefix != NULL && strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool starts_with_ci(const char *s, const char *prefix)
{
    if (s == NULL || prefix == NULL) return false;
    while (*prefix != '\0') {
        char a = *s++;
        char b = *prefix++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

static char *dup_trimmed_value(char *value)
{
    value = trim_in_place_no_strlen(value);
    return strdup(value);
}

static void free_package(Package *pkg)
{
    if (pkg == NULL) return;
    free(pkg->name);
    free(pkg->filename);
    free(pkg->depends);
    memset(pkg, 0, sizeof(*pkg));
}

static bool append_text(char **dst, const char *text)
{
    if (text == NULL || *text == '\0') return true;
    if (*dst == NULL) {
        *dst = strdup(text);
        return *dst != NULL;
    }
    size_t old_len = strlen(*dst);
    size_t add_len = strlen(text);
    char *next = (char *)realloc(*dst, old_len + add_len + 2);
    if (next == NULL) return false;
    next[old_len] = ' ';
    memcpy(next + old_len + 1, text, add_len + 1);
    *dst = next;
    return true;
}

static bool package_valid(const Package *pkg)
{
    return pkg != NULL && pkg->name != NULL && pkg->filename != NULL;
}

static bool repo_add_package(Repo *repo, Package *current)
{
    if (!package_valid(current)) {
        free_package(current);
        return true;
    }
    if (repo->package_count >= XJAPT_MAX_PACKAGES) {
        fprintf(stderr, "xjapt: too many packages in index\n");
        free_package(current);
        return false;
    }
    repo->packages[repo->package_count++] = *current;
    xjapt_serialf("xjapt: found package %s\n", current->name);
    memset(current, 0, sizeof(*current));
    return true;
}

static bool wanted_contains(Wanted *wanted, int wanted_count, const char *name)
{
    for (int i = 0; i < wanted_count; i++) {
        if (strcmp(wanted[i].name, name) == 0) return true;
    }
    return false;
}

static bool wanted_add(Wanted *wanted, int *wanted_count, const char *name)
{
    if (name == NULL || *name == '\0') return true;
    if (wanted_contains(wanted, *wanted_count, name)) return true;
    if (*wanted_count >= XJAPT_MAX_PLAN) {
        fprintf(stderr, "xjapt: dependency list too large\n");
        return false;
    }
    wanted[*wanted_count].name = strdup(name);
    if (wanted[*wanted_count].name == NULL) return false;
    (*wanted_count)++;
    xjapt_serialf("xjapt: want %s\n", name);
    return true;
}

static char *dependency_name(const char *token);
static bool add_dependency_names(Wanted *wanted, int *wanted_count, const char *depends);

static void wanted_free(Wanted *wanted, int wanted_count)
{
    for (int i = 0; i < wanted_count; i++) free(wanted[i].name);
}

static bool scan_package_line_for_wanted(Repo *repo, Wanted *wanted, int wanted_count,
                                         Package *current, bool *capture,
                                         bool *added_package, char *line, char *last_key)
{
    bool continuation = line[0] == ' ' || line[0] == '\t';
    char *s = trim_in_place_no_strlen(line);
    if (*s == '\0') {
        if (*capture) {
            if (!repo_add_package(repo, current)) return false;
            *added_package = true;
        } else {
            free_package(current);
        }
        *capture = false;
        last_key[0] = '\0';
        return true;
    }

    if (*capture && continuation && strcmp(last_key, "Depends") == 0) {
        return append_text(&current->depends, s);
    }

    if (strncmp(s, "Package:", 8) == 0) {
        char *value = trim_in_place_no_strlen(s + 8);
        bool should_capture = wanted_contains(wanted, wanted_count, value) &&
                              find_package(repo, value) == NULL;
        if (!should_capture) {
            *capture = false;
            last_key[0] = '\0';
            return true;
        }
        char *copy = strdup(value);
        if (copy == NULL) return false;
        free(current->name);
        current->name = copy;
        *capture = true;
        strcpy(last_key, "Package");
    } else if (*capture && strncmp(s, "Filename:", 9) == 0) {
        char *value = dup_trimmed_value(s + 9);
        if (value == NULL) return false;
        free(current->filename);
        current->filename = value;
        strcpy(last_key, "Filename");
    } else if (*capture && strncmp(s, "Depends:", 8) == 0) {
        char *value = dup_trimmed_value(s + 8);
        if (value == NULL) return false;
        free(current->depends);
        current->depends = value;
        strcpy(last_key, "Depends");
    } else {
        last_key[0] = '\0';
    }
    return true;
}

static bool scan_packages_for_wanted(Repo *repo, const char *path, Wanted *wanted, int *wanted_count)
{
    xjapt_serialf("xjapt: scan index wanted=%d found=%d\n", *wanted_count, repo->package_count);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "xjapt: cannot open %s\n", path);
        xjapt_serialf("xjapt: cannot open %s\n", path);
        return false;
    }

    Package current = {};
    bool capture = false;
    bool scan_complete = false;
    char line[XJAPT_LINE_MAX];
    int line_len = 0;
    bool line_overflow = false;
    char last_key[32] = {};
    const size_t read_buffer_size = 65536;
    char *buffer = (char *)malloc(read_buffer_size);
    if (buffer == NULL) {
        close(fd);
        return false;
    }

    while (!scan_complete) {
        ssize_t got = read(fd, buffer, read_buffer_size);
        if (got < 0) {
            xjapt_serialf("xjapt: index read error package_count=%d\n", repo->package_count);
            close(fd);
            free(buffer);
            free_package(&current);
            return false;
        }
        if (got == 0) break;

        for (ssize_t i = 0; i < got; i++) {
            char ch = buffer[i];
            if (ch == '\n') {
                bool added_package = false;
                line[line_len] = '\0';
                if (!line_overflow &&
                    !scan_package_line_for_wanted(repo, wanted, *wanted_count,
                                                  &current, &capture, &added_package,
                                                  line, last_key)) {
                    close(fd);
                    free(buffer);
                    free_package(&current);
                    return false;
                }
                if (added_package && repo->package_count > 0) {
                    Package *pkg = &repo->packages[repo->package_count - 1];
                    if (pkg->depends != NULL && !add_dependency_names(wanted, wanted_count, pkg->depends)) {
                        close(fd);
                        free(buffer);
                        return false;
                    }
                }
                line_len = 0;
                line_overflow = false;
                if (!capture && repo->package_count >= *wanted_count) {
                    scan_complete = true;
                    break;
                }
                continue;
            }
            if (line_len < XJAPT_LINE_MAX - 1) {
                line[line_len++] = ch;
            } else {
                line_overflow = true;
            }
        }
    }

    if (line_len > 0 && !line_overflow) {
        bool added_package = false;
        line[line_len] = '\0';
        if (!scan_package_line_for_wanted(repo, wanted, *wanted_count,
                                          &current, &capture, &added_package,
                                          line, last_key)) {
            close(fd);
            free(buffer);
            free_package(&current);
            return false;
        }
        if (added_package && repo->package_count > 0) {
            Package *pkg = &repo->packages[repo->package_count - 1];
            if (pkg->depends != NULL && !add_dependency_names(wanted, wanted_count, pkg->depends)) {
                close(fd);
                free(buffer);
                return false;
            }
        }
    }

    bool ok = true;
    if (capture) {
        ok = repo_add_package(repo, &current);
        if (ok && repo->package_count > 0) {
            Package *pkg = &repo->packages[repo->package_count - 1];
            if (pkg->depends != NULL) ok = add_dependency_names(wanted, wanted_count, pkg->depends);
        }
    } else {
        free_package(&current);
    }
    close(fd);
    free(buffer);
    xjapt_serialf("xjapt: scan done found=%d ok=%d\n", repo->package_count, ok ? 1 : 0);
    return ok;
}

static Package *find_package(Repo *repo, const char *name)
{
    for (int i = 0; i < repo->package_count; i++) {
        if (repo->packages[i].name != NULL && strcmp(repo->packages[i].name, name) == 0) {
            return &repo->packages[i];
        }
    }
    return NULL;
}

static bool plan_contains(Plan *plan, Package *pkg)
{
    for (int i = 0; i < plan->count; i++) {
        if (plan->items[i] == pkg) return true;
    }
    return false;
}

static char *dependency_name(const char *token)
{
    if (token == NULL) return NULL;
    while (*token == ' ' || *token == '\t') token++;
    const char *end = token;
    while (*end != '\0' && *end != ' ' && *end != '\t' &&
           *end != '(' && *end != '|' && *end != ':') {
        end++;
    }
    if (end == token) return NULL;
    size_t len = (size_t)(end - token);
    char *out = (char *)malloc(len + 1);
    if (out == NULL) return NULL;
    memcpy(out, token, len);
    out[len] = '\0';
    return out;
}

static bool add_dependency_names(Wanted *wanted, int *wanted_count, const char *depends)
{
    if (depends == NULL || depends[0] == '\0') return true;
    char *deps = strdup(depends);
    if (deps == NULL) return false;
    char *save = NULL;
    for (char *part = strtok_r(deps, ",", &save); part != NULL; part = strtok_r(NULL, ",", &save)) {
        char *alt = strchr(part, '|');
        if (alt != NULL) *alt = '\0';
        char *dep_name = dependency_name(part);
        if (dep_name != NULL && dep_name[0] != '\0') {
            if (!wanted_add(wanted, wanted_count, dep_name)) {
                free(dep_name);
                free(deps);
                return false;
            }
        }
        free(dep_name);
    }
    free(deps);
    return true;
}

static bool build_needed_repo(Repo *repo, const char *packages_path, const char *root_name)
{
    Wanted wanted[XJAPT_MAX_PLAN] = {};
    int wanted_count = 0;
    if (!wanted_add(wanted, &wanted_count, root_name)) return false;

    int processed_deps = 0;
    while (processed_deps < wanted_count) {
        int before_found = repo->package_count;
        int before_wanted = wanted_count;
        if (!scan_packages_for_wanted(repo, packages_path, wanted, &wanted_count)) {
            wanted_free(wanted, wanted_count);
            return false;
        }
        if (repo->package_count == before_found && before_wanted == wanted_count) {
            xjapt_serialf("xjapt: no progress wanted=%d found=%d\n", wanted_count, repo->package_count);
            break;
        }

        while (processed_deps < repo->package_count) {
            Package *pkg = &repo->packages[processed_deps++];
            if (!add_dependency_names(wanted, &wanted_count, pkg->depends)) {
                wanted_free(wanted, wanted_count);
                return false;
            }
        }
    }

    bool ok = true;
    for (int i = 0; i < wanted_count; i++) {
        if (find_package(repo, wanted[i].name) == NULL) {
            fprintf(stderr, "xjapt: package not found: %s\n", wanted[i].name);
            xjapt_serialf("xjapt: package not found %s\n", wanted[i].name);
            ok = false;
        }
    }
    wanted_free(wanted, wanted_count);
    xjapt_serialf("xjapt: needed repo packages=%d ok=%d\n", repo->package_count, ok ? 1 : 0);
    return ok;
}

static bool resolve_package(Repo *repo, Plan *plan, const char *name, int depth)
{
    if (depth > 64) {
        fprintf(stderr, "xjapt: dependency depth too large at %s\n", name);
        return false;
    }

    Package *pkg = find_package(repo, name);
    if (pkg == NULL) {
        fprintf(stderr, "xjapt: package not found: %s\n", name);
        xjapt_serialf("xjapt: package not found %s\n", name);
        return false;
    }
    if (pkg->visit_state == 2 || plan_contains(plan, pkg)) return true;
    if (pkg->visit_state == 1) return true;
    pkg->visit_state = 1;

    if (pkg->depends != NULL && pkg->depends[0] != '\0') {
        char *deps = strdup(pkg->depends);
        if (deps == NULL) {
            pkg->visit_state = 0;
            return false;
        }
        char *save = NULL;
        for (char *part = strtok_r(deps, ",", &save); part != NULL; part = strtok_r(NULL, ",", &save)) {
            char *alt = strchr(part, '|');
            if (alt != NULL) *alt = '\0';
            char *dep_name = dependency_name(part);
            if (dep_name != NULL && dep_name[0] != '\0') {
                if (!resolve_package(repo, plan, dep_name, depth + 1)) {
                    free(dep_name);
                    free(deps);
                    pkg->visit_state = 0;
                    return false;
                }
            }
            free(dep_name);
        }
        free(deps);
    }

    if (plan->count >= XJAPT_MAX_PLAN) {
        fprintf(stderr, "xjapt: install plan too large\n");
        pkg->visit_state = 0;
        return false;
    }
    plan->items[plan->count++] = pkg;
    pkg->visit_state = 2;
    return true;
}

static int run_child(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execve(argv[0], argv, NULL);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (!WIFEXITED(status)) return -1;
    return WEXITSTATUS(status);
}

static bool parse_http_url(const char *url, UrlParts *out)
{
    if (url == NULL || out == NULL || !starts_with(url, "http://")) return false;
    const char *host_begin = url + 7;
    const char *path_begin = strchr(host_begin, '/');
    const char *host_end = path_begin != NULL ? path_begin : host_begin + strlen(host_begin);
    if (host_end == host_begin) return false;

    const char *port_sep = NULL;
    for (const char *p = host_begin; p < host_end; p++) {
        if (*p == ':') port_sep = p;
    }

    size_t host_len = (size_t)((port_sep != NULL ? port_sep : host_end) - host_begin);
    out->host = (char *)malloc(host_len + 1);
    if (out->host == NULL) return false;
    memcpy(out->host, host_begin, host_len);
    out->host[host_len] = '\0';

    out->port = 80;
    if (port_sep != NULL) {
        out->port = (uint16_t)atoi(port_sep + 1);
        if (out->port == 0) {
            free(out->host);
            out->host = NULL;
            return false;
        }
    }

    out->path = strdup(path_begin != NULL ? path_begin : "/");
    if (out->path == NULL) {
        free(out->host);
        out->host = NULL;
        return false;
    }
    return true;
}

static void free_url_parts(UrlParts *parts)
{
    if (parts == NULL) return;
    free(parts->host);
    free(parts->path);
    memset(parts, 0, sizeof(*parts));
}

static int http_file_callback(const char *data, int len, void *user_data)
{
    DownloadState *state = (DownloadState *)user_data;
    static const char marker[] = "\r\n\r\n";
    for (int i = 0; i < len; i++) {
        char ch = data[i];
        if (!state->saw_header_end) {
            if (!state->status_line_done) {
                if (ch == '\n') {
                    state->status_line[state->status_len] = '\0';
                    state->status_line_done = true;
                    char *space = strchr(state->status_line, ' ');
                    state->status_code = space != NULL ? atoi(space + 1) : 0;
                } else if (ch != '\r' && state->status_len < (int)sizeof(state->status_line) - 1) {
                    state->status_line[state->status_len++] = ch;
                }
            } else {
                if (ch == '\n') {
                    state->header_line[state->header_line_len] = '\0';
                    if (starts_with_ci(state->header_line, "Content-Length:")) {
                        state->content_length = (size_t)strtoull(state->header_line + 15, NULL, 10);
                        state->have_content_length = true;
                    }
                    state->header_line_len = 0;
                } else if (ch != '\r' && state->header_line_len < (int)sizeof(state->header_line) - 1) {
                    state->header_line[state->header_line_len++] = ch;
                }
            }
            if (ch == marker[state->header_match]) {
                state->header_match++;
                if (state->header_match == 4) {
                    state->saw_header_end = true;
                }
            } else {
                state->header_match = (ch == '\r') ? 1 : 0;
            }
            continue;
        }
        size_t body_len = (size_t)(len - i);
        if (write(state->fd, data + i, body_len) != (ssize_t)body_len) return -1;
        state->body_bytes += body_len;
        if (state->body_bytes >= state->next_progress) {
            fprintf(stderr, "xjapt: received %u KiB\n", (unsigned int)(state->body_bytes / 1024));
            state->next_progress = state->body_bytes + 512 * 1024;
        }
        if (state->have_content_length && state->body_bytes >= state->content_length) return 1;
        break;
    }
    return 0;
}

static bool download_http_file(const char *url, const char *out_path)
{
    UrlParts parts = {};
    if (!parse_http_url(url, &parts)) {
        fprintf(stderr, "xjapt: only http:// URLs are supported by builtin downloader: %s\n", url);
        return false;
    }

    int fd = open(out_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "xjapt: cannot create %s\n", out_path);
        free_url_parts(&parts);
        return false;
    }

    DownloadState state = {};
    state.fd = fd;
    state.next_progress = 512 * 1024;
    xhttp_request_t req = {};
    req.method = "GET";
    req.host = parts.host;
    req.port = parts.port;
    req.path = parts.path;
    req.extra_headers = "User-Agent: xjapt/0.1\r\n";

    int ret = xhttp_request(&req, http_file_callback, &state);
    close(fd);
    free_url_parts(&parts);
    bool body_complete = !state.have_content_length || state.body_bytes >= state.content_length;
    bool http_ok = state.saw_header_end && state.status_code >= 200 && state.status_code < 300;
    if (ret == XHTTP_ERR_TIMEOUT && http_ok && body_complete) ret = XHTTP_OK;
    if (ret != XHTTP_OK || !state.saw_header_end || !body_complete ||
        state.status_code < 200 || state.status_code >= 300) {
        fprintf(stderr, "xjapt: http download failed ret=%d status=%d body=%u/%u url=%s\n",
                ret, state.status_code, (unsigned int)state.body_bytes,
                state.have_content_length ? (unsigned int)state.content_length : 0, url);
        unlink(out_path);
        return false;
    }
    return true;
}

static bool ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) return true;
    return mkdir(path, 0755) == 0 || stat(path, &st) == 0;
}

static bool ensure_empty_file(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) return true;
    int fd = open(path, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) return false;
    close(fd);
    return true;
}

static bool prepare_dpkg_database(const char *admindir)
{
    char info[160];
    char updates[160];
    char alternatives[160];
    char status[160];
    char available[160];
    snprintf(info, sizeof(info), "%s/info", admindir);
    snprintf(updates, sizeof(updates), "%s/updates", admindir);
    snprintf(alternatives, sizeof(alternatives), "%s/alternatives", admindir);
    snprintf(status, sizeof(status), "%s/status", admindir);
    snprintf(available, sizeof(available), "%s/available", admindir);

    return ensure_dir(admindir) &&
           ensure_dir(info) &&
           ensure_dir(updates) &&
           ensure_dir(alternatives) &&
           ensure_dir("/var/cache") &&
           ensure_dir("/var/cache/apt") &&
           ensure_dir("/var/cache/apt/archives") &&
           ensure_dir("/var/log") &&
           ensure_empty_file(status) &&
           ensure_empty_file(available);
}

static bool file_exists_nonempty(const char *path)
{
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && st.st_size > 0;
}

static char *join_url(const char *base, const char *name)
{
    size_t bl = strlen(base);
    size_t nl = strlen(name);
    bool slash = bl > 0 && base[bl - 1] == '/';
    char *out = (char *)malloc(bl + nl + (slash ? 1 : 2));
    if (out == NULL) return NULL;
    sprintf(out, "%s%s%s", base, slash ? "" : "/", name);
    return out;
}

static const char *basename_of(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static bool download_file(const char *url, const char *out_path)
{
    fprintf(stderr, "xjapt: download %s\n", url);
    if (starts_with(url, "http://")) return download_http_file(url, out_path);

    char *argv[] = {(char *)"/apps/busybox", (char *)"wget", (char *)"-O", (char *)out_path, (char *)url, NULL};
    int ret = run_child(argv);
    if (ret != 0) {
        fprintf(stderr, "xjapt: wget failed status=%d url=%s\n", ret, url);
        return false;
    }
    return true;
}

static bool fetch_packages_index(const char *base_url, const char *plain_path)
{
    if (file_exists_nonempty(plain_path)) {
        xjapt_serialf("xjapt: use cached index %s\n", plain_path);
        return true;
    }

    char gz_path[160];
    snprintf(gz_path, sizeof(gz_path), "%s.gz", plain_path);
    if (file_exists_nonempty(gz_path)) {
        xjapt_serialf("xjapt: use cached index %s\n", gz_path);
        char *argv[] = {(char *)"/apps/busybox", (char *)"gunzip", (char *)"-f", gz_path, NULL};
        int ret = run_child(argv);
        xjapt_serialf("xjapt: gunzip ret=%d\n", ret);
        return ret == 0;
    }

    char *url = join_url(base_url, "Packages.gz");
    if (url == NULL) return false;
    bool ok = download_file(url, gz_path);
    free(url);
    if (ok) {
        xjapt_serial("xjapt: gunzip start\n");
        char *argv[] = {(char *)"/apps/busybox", (char *)"gunzip", (char *)"-f", gz_path, NULL};
        int ret = run_child(argv);
        xjapt_serialf("xjapt: gunzip ret=%d\n", ret);
        if (ret != 0) {
            fprintf(stderr, "xjapt: gunzip failed status=%d\n", ret);
            return false;
        }
        return true;
    }

    url = join_url(base_url, "Packages");
    if (url == NULL) return false;
    ok = download_file(url, plain_path);
    free(url);
    return ok;
}

static bool download_package(Repo *repo, Package *pkg, char *out_path, size_t out_path_size)
{
    const char *base = basename_of(pkg->filename);
    snprintf(out_path, out_path_size, "%s/cache/%s", XJAPT_CACHE_DIR, base);
    if (file_exists_nonempty(out_path)) {
        xjapt_serialf("xjapt: use cached deb %s\n", out_path);
        return true;
    }

    char *url = NULL;
    if (starts_with(pkg->filename, "http://") || starts_with(pkg->filename, "https://")) {
        url = strdup(pkg->filename);
    } else {
        url = join_url(repo->file_base_url, pkg->filename);
    }
    if (url == NULL) return false;
    bool ok = download_file(url, out_path);
    free(url);
    return ok;
}

static bool install_deb(const char *path)
{
    char admindir_arg[160];
    snprintf(admindir_arg, sizeof(admindir_arg), "--admindir=%s", XJAPT_DPKG_ADMINDIR);
    printf("xjapt: dpkg %s -i %s\n", admindir_arg, path);
    char *argv[] = {(char *)"/usr/bin/dpkg", admindir_arg, (char *)"-i", (char *)path, NULL};
    int ret = run_child(argv);
    if (ret != 0) {
        fprintf(stderr, "xjapt: dpkg failed status=%d path=%s\n", ret, path);
        return false;
    }
    return true;
}

static int xjapt_main(int argc, char **argv)
{
    const char *index_url = XJAPT_DEFAULT_PACKAGES_URL;
    const char *file_base_url = XJAPT_DEFAULT_FILE_BASE_URL;
    const char *package_name = NULL;

    if (argc == 2) {
        package_name = argv[1];
    } else if (argc == 3 || argc == 4) {
        index_url = argv[1];
        package_name = argv[2];
        file_base_url = argc == 4 ? argv[3] : argv[1];
    } else {
        usage();
        return 2;
    }

    if (!ensure_dir("/var") || !ensure_dir("/var/cache") ||
        !ensure_dir(XJAPT_CACHE_DIR) || !ensure_dir("/var/cache/xjapt/cache") ||
        !ensure_dir("/tmp") || !ensure_dir("/tmp/xjapt")) {
        fprintf(stderr, "xjapt: cannot create xjapt work directories\n");
        return 1;
    }
    if (!prepare_dpkg_database(XJAPT_DPKG_ADMINDIR)) {
        fprintf(stderr, "xjapt: cannot prepare dpkg database\n");
        return 1;
    }

    const char *packages_path = "/var/cache/xjapt/Packages";
    fprintf(stderr, "xjapt: index %s\n", index_url);
    fprintf(stderr, "xjapt: files %s\n", file_base_url);
    if (!fetch_packages_index(index_url, packages_path)) return 1;

    Repo *repo = (Repo *)calloc(1, sizeof(Repo));
    if (repo == NULL) {
        fprintf(stderr, "xjapt: cannot allocate package index\n");
        return 1;
    }
    repo->index_url = (char *)index_url;
    repo->file_base_url = (char *)file_base_url;
    if (!build_needed_repo(repo, packages_path, package_name)) return 1;
    fprintf(stderr, "xjapt: loaded %d needed packages\n", repo->package_count);

    Plan plan = {};
    if (!resolve_package(repo, &plan, package_name, 0)) return 1;
    fprintf(stderr, "xjapt: install plan:");
    for (int i = 0; i < plan.count; i++) fprintf(stderr, " %s", plan.items[i]->name);
    fprintf(stderr, "\n");

    for (int i = 0; i < plan.count; i++) {
        char deb_path[256];
        if (!download_package(repo, plan.items[i], deb_path, sizeof(deb_path))) return 1;
        if (!install_deb(deb_path)) return 1;
    }

    fprintf(stderr, "xjapt: done\n");
    return 0;
}

extern "C" int xjapt_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int xjapt_main_cpp(int argc, char *argv[], char *envp[])
{
    (void)envp;
    return xjapt_main(argc, argv);
}
