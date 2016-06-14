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
    assert(signal(SIGINT, interrupted) != SIG_ERR);
}

