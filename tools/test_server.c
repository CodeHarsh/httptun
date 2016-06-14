#include <stdio.h>
#include "../src/server.h"
#include "../src/sig_handle.h"
#include "../src/log.h"
#include "echo_server.h"
#include "echo_client.h"

#include <unistd.h>
#include <assert.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern const char *__progname;

#define UDS_SOCK_PATH "/tmp/server.sock"

int do_stop = 0;

void read_write(int fd) {
    log_info("server", "client fd: %d", fd);
    char buf[100] = {'f', 'o', 'o'};
    int rc;
    write(fd, buf, 3);
    rc=read(fd,buf,sizeof(buf));
    log_info("client", "Received %d bytes: %s", rc, buf);
}

int main() {
    log_init(3, __progname);
    uds_server_t *s;
    s = start_uds_server(UDS_SOCK_PATH);
    assert(s != NULL);
    usleep(1000 * 100);
    int fd = connect_to_echo_server(UDS_SOCK_PATH);

    log_info("main", "Connected to echo-server: %d", fd);
    do_stop = 0;
    stop_on_sigint();
    log_debug("main", "Starting httptun-server");
    run_server(9090, fd);
    log_debug("main", "Terminating test");

    //read_write(fd);
    close(fd);
    stop_uds_server(s);
    return 0;
}
