#include "sig_handle.h"
#include "stop.h"

#include <signal.h>
#include <assert.h>
#include <stdlib.h>

int do_stop;

static void interrupted(int sig) {
    do_stop = 1;
}

void stop_on_sigint() {
    struct sigaction sig;
    sig.sa_handler = &interrupted;
    sigemptyset(&sig.sa_mask);
    assert(sigaction(SIGINT, &sig, NULL) == 0);
}

