#include <stdio.h>
#include "../src/client.h"

#include <unistd.h>
#include <assert.h>

#include <stdio.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

int do_stop = 0;


int main() {
    int fd = open("/tmp/test.client", O_RDWR);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    run_client("test-3", 9090, fd, "foo", "bar", 0);
    close(fd);
    return 0;
}
