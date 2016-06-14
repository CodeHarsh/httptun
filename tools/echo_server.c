#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "../src/log.h"
#include "echo_server.h"

#define ES "echo_server"

struct uds_server_s {
    pthread_t server_thd;
    char *sock_path;
    int do_stop;
};

static void *run_uds_server(void *server_) {
    uds_server_t *s = (uds_server_t *) server_;
    struct sockaddr_un addr;
    char buf[100];
    int fd,cl,rc;

    log_info(ES, "Using socket-path: %s", s->sock_path);

    if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        fatal(ES, "socket error");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, s->sock_path, sizeof(addr.sun_path)-1);

    unlink(s->sock_path);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        fatal(ES, "bind error");
    }

    if (listen(fd, 5) == -1) {
        fatal(ES, "listen error");
    }

    while (__sync_fetch_and_and(&s->do_stop, 0xffffffff) == 0) {
        if ( (cl = accept(fd, NULL, NULL)) == -1) {
            log_warn(ES, "accept error");
            continue;
        }
        log_debug(ES, "Accepted a new connection");

        while ( (rc=read(cl, buf, sizeof(buf))) > 0) {
            log_info(ES, "read %u bytes: %.*s\n", rc, rc, buf);
            int x = write(cl, "REPLY: ", 7);
            x += write(cl, buf, rc);
            log_info(ES, "wrote: %d bytes\n", x);
        }
        if (rc == -1) {
            fatal(ES, "read");
        }
        else if (rc == 0) {
            log_info(ES, "EOF\n");
            close(cl);
        }
    }
}

void stop_uds_server(uds_server_t *s) {
    __sync_fetch_and_add(&(s->do_stop), 1);
    pthread_join(s->server_thd, NULL);
    free(s->sock_path);
    free(s);
}

uds_server_t *start_uds_server(const char *socket_path) {
    uds_server_t *s = calloc(1, sizeof(uds_server_t));
    s->sock_path = strdup(socket_path);
    assert(s != NULL);
    assert(pthread_create(&s->server_thd, NULL, run_uds_server, (void *)s) == 0);
    return s;
}
