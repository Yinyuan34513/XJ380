#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "xapi/include/libsys.h"
#include "xapi/include/xposix/syscall_ret.h"

static constexpr uint64_t SYS_UTIMENSAT_NR = 280;

static void ok(const char *msg, unsigned long len)
{
    write(1, msg, len);
}

static void fail(const char *msg, unsigned long len)
{
    write(2, msg, len);
}

#define OK(msg) ok(msg, sizeof(msg) - 1)
#define FAIL(msg) fail(msg, sizeof(msg) - 1)

static int posixdemo_main_impl()
{
    OK("posixdemo: start\n");

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        OK("clock_gettime ok\n");
    } else {
        FAIL("clock_gettime failed\n");
    }

    const char *path = "/posixdemo.tmp";
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        FAIL("open failed\n");
        return 1;
    }

    const char payload[] = "hello from xapi posix\n";
    if (write(fd, payload, sizeof(payload) - 1) < 0) {
        FAIL("write failed\n");
        close(fd);
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) == 0) {
        OK("fstat ok\n");
    } else {
        FAIL("fstat failed\n");
    }

    struct timespec file_times[2] = {{1585591680, 0}, {1585591680, 0}};
    long raw_utime = __xposix_ret(enter_syscall(SYS_UTIMENSAT_NR, fd, 0, (uint64_t)file_times, 0, 0, 0));
    if (raw_utime == 0) {
        OK("utimensat fd/null ok\n");
    } else {
        FAIL("utimensat fd/null failed\n");
    }

    lseek(fd, 0, SEEK_SET);
    char buf[64] = {};
    if (read(fd, buf, sizeof(buf) - 1) >= 0) {
        OK("read ok\n");
    } else {
        FAIL("read failed\n");
    }
    close(fd);

    struct iovec iov[2];
    const char left[] = "writev ";
    const char right[] = "ok\n";
    iov[0].iov_base = (void *)left;
    iov[0].iov_len = sizeof(left) - 1;
    iov[1].iov_base = (void *)right;
    iov[1].iov_len = sizeof(right) - 1;
    writev(1, iov, 2);

    int pipefd[2];
    if (pipe(pipefd) == 0) {
        const char byte = 'x';
        struct pollfd pipe_poll = {.fd = pipefd[0], .events = POLLIN, .revents = 0};
        bool pipe_ready = write(pipefd[1], &byte, 1) == 1 && poll(&pipe_poll, 1, 0) == 1 &&
                          (pipe_poll.revents & POLLIN) != 0 && read(pipefd[0], &buf[0], 1) == 1;
        close(pipefd[1]);
        pipe_poll.revents = 0;
        bool pipe_eof = poll(&pipe_poll, 1, 0) == 1 && (pipe_poll.revents & POLLHUP) != 0 &&
                        read(pipefd[0], &buf[0], 1) == 0;
        close(pipefd[0]);
        if (pipe_ready && pipe_eof) {
            OK("pipe/poll/hup ok\n");
        } else {
            FAIL("pipe/poll/hup failed\n");
        }
    } else {
        FAIL("pipe failed\n");
    }

    int fork_pipefd[2];
    if (pipe(fork_pipefd) == 0) {
        pid_t child = fork();
        if (child == 0) {
            close(fork_pipefd[0]);
            const char byte = 'f';
            bool wrote = write(fork_pipefd[1], &byte, 1) == 1;
            close(fork_pipefd[1]);
            _exit(wrote ? 0 : 1);
        }

        bool fork_eof = false;
        if (child > 0) {
            close(fork_pipefd[1]);
            char byte = 0;
            int status = 0;
            fork_eof = read(fork_pipefd[0], &byte, 1) == 1 && byte == 'f' &&
                       read(fork_pipefd[0], &byte, 1) == 0 && waitpid(child, &status, 0) == child && status == 0;
            close(fork_pipefd[0]);
        } else {
            close(fork_pipefd[0]);
            close(fork_pipefd[1]);
        }

        if (fork_eof) {
            OK("pipe/fork/eof ok\n");
        } else {
            FAIL("pipe/fork/eof failed\n");
        }
    } else {
        FAIL("pipe/fork setup failed\n");
    }

    int nonblock_pipefd[2];
    if (pipe2(nonblock_pipefd, O_NONBLOCK) == 0) {
        char pipe_fill[1024] = {};
        const char byte = 'x';
        bool pipe_nonblock = true;
        for (int i = 0; i < 128; i++) {
            if (write(nonblock_pipefd[1], pipe_fill, sizeof(pipe_fill)) < 0) {
                pipe_nonblock = errno == EAGAIN;
                break;
            }
            if (i == 127) pipe_nonblock = false;
        }
        pipe_nonblock = pipe_nonblock && write(nonblock_pipefd[1], &byte, 1) == -1 && errno == EAGAIN;
        close(nonblock_pipefd[0]);
        close(nonblock_pipefd[1]);
        if (pipe_nonblock) {
            OK("pipe/nonblock ok\n");
        } else {
            FAIL("pipe/nonblock failed\n");
        }
    } else {
        FAIL("pipe/nonblock setup failed\n");
    }

    DIR *dir = opendir("/");
    if (dir != NULL) {
        readdir(dir);
        closedir(dir);
        OK("dirent ok\n");
    } else {
        FAIL("opendir failed\n");
    }

    void *mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem != MAP_FAILED) {
        mprotect(mem, 4096, PROT_READ);
        munmap(mem, 4096);
        OK("mmap ok\n");
    } else {
        FAIL("mmap failed\n");
    }

    unlink(path);
    OK("posixdemo: done\n");
    return 0;
}

extern "C" int posixdemo_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int posixdemo_main_cpp(int, char **, char **)
{
    return posixdemo_main_impl();
}
