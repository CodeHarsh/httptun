#ifndef _CLIENT_H
#define _CLIENT_H

#if HAVE_CONFIG_H
#  include <config.h>
#endif

void run_client(const char* host, int port, int tun_fd, const char *username, const char *password, int use_ssl);
#endif
