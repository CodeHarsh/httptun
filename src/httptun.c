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
#include "sig_handle.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
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
    fprintf(stderr, " -b, --bridgeHost <hostname | ip>   host to bridge to [client mode only]\n");
    fprintf(stderr, " -c, --command '<command>'          command to set up tunnel (TUN-UP command)\n");
    fprintf(stderr, " -K, --sslKeyFile <path>            server key file [server mode only]\n");
    fprintf(stderr, " -C, --sslCertFile <path>           server cert file [server mode only]\n");
    fprintf(stderr, " -S, --ssl                          use SSL [client mode only] **doesn't verify peer**\n");
    fprintf(stderr, " -U, --username <username>          \n");
    fprintf(stderr, " -P, --password <password>          \n");
	fprintf(stderr, "\n");
	fprintf(stderr, "see manual page " PACKAGE "(8) for more information\n");
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = malloc(fsize + 1);
    fread(data, fsize, 1, f);
    data[fsize] = 0;

    fclose(f);
    return data;
}

int main(int argc, char *argv[]) {
	int debug = 1;
	int ch;

    int server = 0;
    int port = 8080;
    char *bridge_host = NULL;
    char *tun_up_cmd = NULL;
    char *username = NULL;
    char *password = NULL;
    char *ssl_key = NULL;
    char *ssl_cert = NULL;
    int use_ssl = 0;
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
        { "command", required_argument, 0, 'c'},
        { "username", required_argument, 0, 'U'},
        { "password", required_argument, 0, 'P'},
        { "sslKeyFile", required_argument, 0, 'K'},
        { "sslCertFile", required_argument, 0, 'C'},
        { "ssl", no_argument, 0, 'S'},
        { 0 }
	};
	while (1) {
		int option_index = 0;
		ch = getopt_long(argc, argv, "hvdD:sp:b:c:U:P:K:C:S",
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
        case 'c':
            tun_up_cmd = strdup(optarg);
            break;
        case 'U':
            username = strdup(optarg);
            break;
        case 'P':
            password = strdup(optarg);
            break;
        case 'K':
            ssl_key = read_file(optarg);
            break;
        case 'C':
            ssl_cert = read_file(optarg);
            break;
        case 'S':
            use_ssl = 1;
            break;
		default:
			fprintf(stderr, "unknown option `%c'\n", ch);
			usage();
			exit(1);
		}
	}

	log_init(debug, __progname);

    if (tun_up_cmd == NULL) fatalx("TUN-UP command must be provided");

    if (username == NULL || password == NULL) fatalx("Both username and password must be provided");

	/* TODO:3000 It's time for you program to do something. Add anything
	 * TODO:3000 you want here. */
    log_debug("main", "Allocating tun");
    int tun_fd = alloc_tun(tun_up_cmd);
    assert(tun_fd > 0);
    if (server) {
        if (bridge_host != NULL) fatalx("Server doesn't bridge-over, it _is_ the bridgeHost");
        if ((ssl_key == NULL && ssl_cert != NULL) ||
            (ssl_key != NULL && ssl_cert == NULL)) fatalx("Either none or both ssl-cert and ssl-key files must be provided");
        if (use_ssl != 0) fatalx("Use 'ssl' is a client-mode option");
        run_server(port, tun_fd, username, password, ssl_key, ssl_cert);
    } else {
        if (bridge_host == NULL) fatalx("Client requires bridgeHost");
        if (ssl_key != NULL || ssl_cert != NULL) fatalx("SSL key and cert are server-mode properties");
        run_client(bridge_host, port, tun_fd, username, password, use_ssl);
    }
    log_debug("main", "Closing tun");
    close(tun_fd);
    free(tun_up_cmd);
    free(username);
    free(password);
    free(ssl_key);
    free(ssl_cert);
    free(bridge_host);

	return EXIT_SUCCESS;
}
