/* utc_man.c
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <stdarg.h>

// #include <pthread.h>

#include <fcntl.h>

#include <time.h>

#include <errno.h>

#include <signal.h>

// #include "utc_man.h"

#define PORT    8080
#define BACKLOG 5
#define DEBUG

#define HELLO_MSG   "hello from server\r\n"
#define SLEEP_SEC_MAX   20
#define MSG_BOUNDARY    "--msg-boundary"
#define CRLF        "\r\n"

#ifdef DEBUG
#define LOG_FILE    "/home/shihaol/tmp/utc_man.log"
#define LOG_BANNER          "\n\n====================================\n"
#define LOG_SEPRATE_LINE    "\n------------------------------------\n"
static inline void msglog(const char *fmt, ...)
{
    int n = 0;
    FILE *fp = NULL;
    va_list ap;

    fp = fopen(LOG_FILE, "a");
    if (fp)
    {
        va_start(ap, fmt);
        vfprintf(fp, fmt, ap);
        va_end(ap);
        fclose(fp);
    }
    else
    {
        printf("%d: %s\n", errno, strerror(errno));
    }
}
#else
#define msglog(fmt, ...)
#endif

// modify needed!
static int conn_fd = 0;

static int get_random_seconds()
{
    return rand() % SLEEP_SEC_MAX + 1;
}

static char *build_message(time_t ts)
{
    const char *msg_format = MSG_BOUNDARY CRLF "%d" CRLF;
    char *buf = NULL;
    size_t size = 0;
    int n = 0;

    n = snprintf(buf, size, msg_format, (int)ts);
    if (n <= 0)
        return NULL;

    size = (size_t)n + 1;
    buf = malloc(size);
    if (!buf)
        return NULL;

    n = snprintf(buf, size, msg_format, (int)ts);
    if (n <= 0)
    {
        free(buf);
        return NULL;
    }

    return buf;
}

static int full_write(int fd, const char *buf, int size)
{
    int n = 0;
    int left = size;

    while (left > 0)
    {
        n = write(fd, buf, left);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                continue;
            }
            else
            {
                return n;
            }
        }
        buf += n;
        left -= n;
    }

    return size - left;
}

static void do_work(int fd)
{
    int ret = 0;
    int len;
    int n = 0;
    time_t ts;
    char *pmsg = NULL;

    // first, say hello
    msglog("fd %d worker thread says hi\n", fd);
    len = strlen(HELLO_MSG);
    ret = full_write(fd, HELLO_MSG, len);
    if (ret < 0)
    {
        msglog("fd %d write failed: (%d) %s\n", fd, errno, strerror(errno));
        goto failed;
    }

    // then, send the UTC time after sleep random seconds repeatedly
    srand(time(NULL));
    while (1)
    {
        n = get_random_seconds();
        msglog("fd %d worker thread will sleep %d seconds\n", fd, n);
        sleep(n);
        ts = time(NULL);
        pmsg = build_message(ts);
        if (pmsg)
        {
            msglog("fd %d worker thread send UTC %d\n", fd, (int)ts);
            len = strlen(pmsg);
            ret = full_write(fd, pmsg, len);
            if (ret < 0)
            {
                msglog("fd %d write failed: (%d) %s\n", fd, errno, strerror(errno));
                free(pmsg);
                goto failed;
            }
            free(pmsg);
        }
        else
        {
            msglog("fd %d build msg failed\n", fd);
            goto failed;
        }
    }

failed:
    close(fd);
}

static int run_server(void)
{
    int ret = 0;
    int sock = -1, conn = -1;
    struct sockaddr_in servaddr = {0};
    struct sockaddr_in cliaddr = {0};
    socklen_t addrlen;
    int reuse_addr = 1;

    msglog(LOG_BANNER);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        msglog("create socket failed\n");
        return -1;
    }
    
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    ret = bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret < 0)
    {
        msglog("bind failed\n");
        return -1;
    }

    ret = listen(sock, BACKLOG);
    if (ret < 0)
    {
        msglog("bind failed\n");
        return -1;
    }

    msglog("server listening at port %d...\n", PORT);

    // ignore sigpipe
    signal(SIGPIPE, SIG_IGN);

    while (1)
    {
        msglog(LOG_SEPRATE_LINE);
        conn = accept(sock, (struct sockaddr *)&cliaddr, &addrlen);
        if (conn < 0)
        {
            msglog("accept failed\n");
        }

        msglog("accept incomming connection with fd %d\n", conn);
        do_work(conn);
    }

}

int main(void)
{
    run_server();
    return 0;
}