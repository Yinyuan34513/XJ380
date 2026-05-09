#include <errno.h>
#include <stdio.h>
#include <string.h>

static int open_output(FILE **fp, char *path, size_t path_size, const char *root, const char *name)
{
    if (snprintf(path, path_size, "%s/%s", root, name) >= (int)path_size) {
        fprintf(stderr, "opkg-conf-gen: output path is too long\n");
        return -1;
    }

    *fp = fopen(path, "w");
    if (*fp == NULL) {
        fprintf(stderr, "opkg-conf-gen: failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

static int write_base_config(FILE *fp)
{
    static const char *const lines[] = {
        "dest root /\n",
        "lists_dir ext /usr/lib/opkg/lists\n",
        "arch all 1\n",
        "arch noarch 1\n",
        "arch x86_64 10\n",
        "option overlay_root /usr/lib/opkg\n",
        "option ssl_ca_file /etc/ssl/certs/ca-certificates.crt\n",
    };
    size_t i;

    for (i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        if (fputs(lines[i], fp) == EOF)
            return -1;
    }

    return 0;
}

static int write_feeds(FILE *fp, int argc, char **argv)
{
    int i;

    for (i = 2; i < argc; i += 2) {
        if (fprintf(fp, "src/gz %s %s\n", argv[i], argv[i + 1]) < 0)
            return -1;
    }

    return 0;
}

static int write_config_file(const char *root, const char *name, int argc, char **argv)
{
    char path[4096];
    FILE *fp;

    if (open_output(&fp, path, sizeof(path), root, name) != 0)
        return -1;

    if (write_base_config(fp) != 0 || write_feeds(fp, argc, argv) != 0) {
        fprintf(stderr, "%s: failed to write %s: %s\n", argv[0], path, strerror(errno));
        fclose(fp);
        return -1;
    }

    if (fclose(fp) == EOF) {
        fprintf(stderr, "%s: failed to close %s: %s\n", argv[0], path, strerror(errno));
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2 || ((argc - 2) % 2) != 0) {
        fprintf(stderr, "usage: %s <output-dir> [feed-name feed-url]...\n", argv[0]);
        return 2;
    }

    if (write_config_file(argv[1], "etc/opkg.conf", argc, argv) != 0)
        return 1;

    if (write_config_file(argv[1], "etc/opkg/opkg.conf", argc, argv) != 0)
        return 1;

    return 0;
}
