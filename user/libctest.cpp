#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_failures = 0;

static void out(const char *s)
{
    write(1, s, strlen(s));
}

static void expect(int cond, const char *name)
{
    if (cond) {
        out("ok ");
        out(name);
        out("\n");
    } else {
        out("FAIL ");
        out(name);
        out("\n");
        g_failures++;
    }
}

static int same(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static int libctest_main_impl()
{
    out("libctest: start\n");

    char mem[16];
    memset(mem, 'A', sizeof(mem));
    mem[15] = 0;
    expect(mem[0] == 'A' && mem[14] == 'A', "memset");

    memcpy(mem, "abcdef", 7);
    memmove(mem + 2, mem, 5);
    expect(memcmp(mem, "ababcde", 7) == 0, "memmove overlap forward");

    memcpy(mem, "abcdef", 7);
    memmove(mem, mem + 2, 5);
    expect(same(mem, "cdef"), "memmove overlap backward");

    memcpy(mem, "abcdef", 7);
    expect(memchr(mem, 'd', 6) == mem + 3, "memchr");
    bzero(mem, sizeof(mem));
    expect(mem[0] == 0 && mem[15] == 0, "bzero");

    expect(strnlen("abcdef", 3) == 3, "strnlen limited");
    expect(strnlen("abc", 8) == 3, "strnlen terminated");
    expect(strcasecmp("ToyBox", "toybox") == 0, "strcasecmp");
    expect(strncasecmp("ToyBox", "toycar", 3) == 0, "strncasecmp");
    expect(strspn("abc123", "abc") == 3, "strspn");
    expect(strcspn("abc123", "123") == 3, "strcspn");
    expect(strpbrk("abc123", "32") != NULL && *strpbrk("abc123", "32") == '2', "strpbrk");
    expect(index("abc", 'b') != NULL && *index("abc", 'b') == 'b', "index");
    expect(rindex("abca", 'a') != NULL && rindex("abca", 'a') == "abca" + 3, "rindex");

    char *dup = strdup("hello");
    expect(dup != NULL && same(dup, "hello"), "strdup");
    free(dup);

    char *ndup = strndup("hello", 3);
    expect(ndup != NULL && same(ndup, "hel"), "strndup");
    free(ndup);

    char tokbuf[] = "aa,bb,,cc";
    char *save = NULL;
    char *a = strtok_r(tokbuf, ",", &save);
    char *b = strtok_r(NULL, ",", &save);
    char *c = strtok_r(NULL, ",", &save);
    char *d = strtok_r(NULL, ",", &save);
    expect(a && b && c && d == NULL && same(a, "aa") && same(b, "bb") && same(c, "cc"), "strtok_r");

    expect(isalpha('A') && isalpha('z') && !isalpha('1'), "isalpha");
    expect(isdigit('7') && !isdigit('x'), "isdigit");
    expect(isspace(' ') && isspace('\n') && !isspace('x'), "isspace");
    expect(tolower('A') == 'a' && toupper('z') == 'Z', "case convert");
    expect(isxdigit('f') && isxdigit('F') && isxdigit('9') && !isxdigit('g'), "isxdigit");

    errno = ENOENT;
    expect(__errno_location() == &errno, "__errno_location");
    expect(same(strerror(errno), "No such file or directory"), "strerror");
    perror("libctest perror sample");

    if (g_failures == 0) {
        out("libctest: done\n");
        return 0;
    }

    out("libctest: failed\n");
    return 1;
}

extern "C" int libctest_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int libctest_main_cpp(int, char **, char **)
{
    return libctest_main_impl();
}
