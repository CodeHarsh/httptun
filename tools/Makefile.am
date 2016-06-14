noinst_PROGRAMS = test_client test_server

test_client_SOURCES  = test_client.c
test_client_CPPFLAGS = $(AM_CFLAGS) @MICROHTTPD_CFLAGS@ @CURL_CFLAGS@
test_client_LDADD = $(AM_LDFLAGS) @MICROHTTPD_LIBS@ @CURL_LIBS@ ../src/libhttptun.la

test_server_SOURCES  = test_server.c echo_server.h echo_server.c echo_client.h echo_client.c
test_server_CPPFLAGS = $(AM_CFLAGS) @MICROHTTPD_CFLAGS@ @CURL_CFLAGS@ @PTHREADS_CFLAGS@
test_server_LDADD = $(AM_LDFLAGS) @MICROHTTPD_LIBS@ @CURL_LIBS@ @PTHREADS_LIBS@ ../src/libhttptun.la
