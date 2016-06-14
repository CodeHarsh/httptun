#include "client.h"
#include "stop.h"
#include "log.h"
#include <unistd.h>

void run_client(const char* host, int port, int tun_fd) {
    while(! do_stop) {
        log_info("client", "Let it go.. Let it go...!");
        sleep(1);
    }
}
