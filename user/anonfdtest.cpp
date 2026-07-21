#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/eventfd.h>
#include <sys/memfd.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

static int g_failures = 0;

static void out(const char *s)
{
    write(1, s, strlen(s));
}

static void expect(bool ok, const char *name)
{
    if (ok) {
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

static int anonfdtest_main_impl()
{
    out("anonfdtest: start\n");

    struct pollfd ignored_fd = {-1, POLLIN, (short)0x7fff};
    expect(poll(&ignored_fd, 1, 0) == 0 && ignored_fd.revents == 0, "poll ignores negative fd");

    struct pollfd invalid_fd = {32767, POLLIN, 0};
    expect(poll(&invalid_fd, 1, 0) == 1 && (invalid_fd.revents & POLLNVAL), "poll reports invalid fd");

    struct timespec zero_timeout = {};
    expect(ppoll(&ignored_fd, 1, &zero_timeout, NULL) == 0 && ignored_fd.revents == 0,
           "ppoll zero timeout");

    int efd = eventfd(2, 0);
    expect(efd >= 0, "eventfd create");
    if (efd >= 0) {
        uint64_t one = 3;
        uint64_t got = 0;
        expect(write(efd, &one, sizeof(one)) == (ssize_t)sizeof(one), "eventfd write");
        expect(read(efd, &got, sizeof(got)) == (ssize_t)sizeof(got) && got == 5, "eventfd read");
        close(efd);
    }

    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    expect(tfd >= 0, "timerfd create");
    if (tfd >= 0) {
        struct itimerspec spec = {};
        spec.it_value.tv_sec = 0;
        spec.it_value.tv_nsec = 1000000;
        expect(timerfd_settime(tfd, 0, &spec, NULL) == 0, "timerfd settime");
        struct pollfd pfd = {tfd, POLLIN, 0};
        expect(poll(&pfd, 1, 50) > 0 && (pfd.revents & POLLIN), "timerfd poll");
        uint64_t expirations = 0;
        expect(read(tfd, &expirations, sizeof(expirations)) == (ssize_t)sizeof(expirations) && expirations >= 1,
               "timerfd read");
        close(tfd);
    }

    sigset_t mask = 1ULL << SIGUSR1;
    expect(sigprocmask(SIG_BLOCK, &mask, NULL) == 0, "sigprocmask block");
    int sfd = signalfd(-1, &mask, 0);
    expect(sfd >= 0, "signalfd create");
    if (sfd >= 0) {
        expect(kill(getpid(), SIGUSR1) == 0, "signalfd kill self");
        struct pollfd pfd = {sfd, POLLIN, 0};
        expect(poll(&pfd, 1, 50) > 0 && (pfd.revents & POLLIN), "signalfd poll");
        struct signalfd_siginfo info = {};
        expect(read(sfd, &info, sizeof(info)) == (ssize_t)sizeof(info) && info.ssi_signo == SIGUSR1,
               "signalfd read");
        close(sfd);
    }
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    int mfd = memfd_create("anonfd", 0);
    expect(mfd >= 0, "memfd create");
    if (mfd >= 0) {
        const char payload[] = "memfd payload";
        expect(write(mfd, payload, sizeof(payload)) == (ssize_t)sizeof(payload), "memfd write");
        void *map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
        expect(map != MAP_FAILED, "memfd mmap");
        if (map != MAP_FAILED) {
            expect(memcmp(map, payload, sizeof(payload)) == 0, "memfd mmap contents");
            ((char *)map)[0] = 'M';
            lseek(mfd, 0, SEEK_SET);
            char buf[32] = {};
            expect(read(mfd, buf, sizeof(payload)) == (ssize_t)sizeof(payload) && buf[0] == 'M',
                   "memfd shared view");
            munmap(map, 4096);
        }
        close(mfd);
    }

    out(g_failures == 0 ? "anonfdtest: done\n" : "anonfdtest: failed\n");
    return g_failures == 0 ? 0 : 1;
}

extern "C" int anonfdtest_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int anonfdtest_main_cpp(int, char **, char **)
{
    return anonfdtest_main_impl();
}
