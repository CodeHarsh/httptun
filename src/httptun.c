/* -*- mode: c; c-file-style: "openbsd" -*- */
/* TODO:5002 You may want to change the copyright of all files. This is the
 * TODO:5002 ISC license. Choose another one if you want.
 */
/*
 * Copyright (c) 2014 Janmejay Singh <singh.janmejay@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "httptun.h"
#include "stop.h"
#include "server.h"
#include "tun.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <assert.h>
#include <client.h>

extern const char *__progname;

static void
usage(void)
{
	/* TODO:3002 Don't forget to update the usage block with the most
	 * TODO:3002 important options. */
	fprintf(stderr, "Usage: %s [OPTIONS]\n",
	    __progname);
	fprintf(stderr, "Version: %s\n", PACKAGE_STRING);
	fprintf(stderr, "\n");
	fprintf(stderr, " -d, --debug                        be more verbose.\n");
	fprintf(stderr, " -h, --help                         display help and exit\n");
	fprintf(stderr, " -v, --version                      print version and exit\n");
	fprintf(stderr, " -s, --server                       run a server (tunnel terminator)\n");
    fprintf(stderr, " -p, --port <port>                  server port number\n");
    fprintf(stderr, " -b, --bridgeHost <hostname | ip>   host to bridge to\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "see manual page " PACKAGE "(8) for more information\n");
}

int do_stop;

static void interrupted(int sig) {
    do_stop = 1;
}

static void stop_on_sigint() {
    struct sigaction sig;
    sig.sa_handler = &interrupted;
    sigemptyset(&sig.sa_mask);
    assert(sigaction (SIGINT, &sig, NULL) == 0);
}

int
main(int argc, char *argv[])
{
	int debug = 1;
	int ch;

    int server = 0;
    int port = 8080;
    char* bridge_host = NULL;
    do_stop = 0;
    stop_on_sigint();

	/* TODO:3001 If you want to add more options, add them here. */
	static struct option long_options[] = {
        { "debug", no_argument, 0, 'd' },
        { "help",  no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        { "server", no_argument, 0, 's'},
        { "port", required_argument, 0, 'p'},
        { "bridgeHost", required_argument, 0, 'b'},
        { 0 }
	};
	while (1) {
		int option_index = 0;
		ch = getopt_long(argc, argv, "hvdD:sp:b:",
		    long_options, &option_index);
		if (ch == -1) break;
		switch (ch) {
		case 'h':
			usage();
			exit(0);
			break;
		case 'v':
			fprintf(stdout, "%s\n", PACKAGE_VERSION);
			exit(0);
			break;
		case 'd':
			debug++;
			break;
		case 'D':
			log_accept(optarg);
			break;
        case 's':
            server = 1;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'b':
            bridge_host = strdup(optarg);
            break;
		default:
			fprintf(stderr, "unknown option `%c'\n", ch);
			usage();
			exit(1);
		}
	}

	log_init(debug, __progname);

	/* TODO:3000 It's time for you program to do something. Add anything
	 * TODO:3000 you want here. */
    log_debug("main", "Allocating tun");
    int tun_fd = alloc_tun();
    if (server) {
        if (bridge_host != NULL) {
            log_crit("main", "Server doesn't bridge-over, it _is_ the bridgeHost");
        } else {
            run_server(port, tun_fd);
        }
    } else {
        if (bridge_host == NULL) {
            log_crit("main", "Client requires bridgeHost");
        } else {
            run_client(bridge_host, port, tun_fd);
            free(bridge_host);
        }
    }
    log_debug("main", "Closing tun");
    close(tun_fd);

	return EXIT_SUCCESS;
}
