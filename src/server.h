#ifndef _SERVER_H
#define _SERVER_H

#if HAVE_CONFIG_H
#  include <config.h>
#endif

void run_server(int port, int tun_fd,
                const char *username, const char *password,
                const char *ssl_key, const char *ssl_cert);
#endif
