#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../src/log.h"

#define EC "echo_client"

int connect_to_echo_server(const char *uds_sock) {
    struct sockaddr_un addr;
    int fd;

    log_info(EC, "Using socket-path: %s", uds_sock);

    if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        fatal(EC, "socket error");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, uds_sock, sizeof(addr.sun_path)-1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        fatal(EC, "connect error");
    }

    return fd;
}

/* int main(int argc, char *argv[]) { */
/*     char buf[100]; */
/*     log_init(3, __progname); */

/*     while( (rc=read(STDIN_FILENO, buf, sizeof(buf))) > 0) { */
/*         log_debug(EC, "read: %d bytes, will now try to write", rc); */
/*         if (write(fd, buf, rc) != rc) { */
/*             if (rc > 0) { */
/*                 log_info(EC, "partial write"); */
/*             } else { */
/*                 fatal(EC, "write error"); */
/*             } */
/*         } */
/*         usleep(1000 * 10); */
/*         rc=read(fd,buf,sizeof(buf)); */
/*         log_info(EC, "Received %d bytes: %s", rc, buf); */
/*     } */
/*     return 0; */
/* } */
