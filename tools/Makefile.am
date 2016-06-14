noinst_PROGRAMS = test_client

test_client_SOURCES  = test_client.c
test_client_CPPFLAGS = $(AM_CFLAGS) @MICROHTTPD_CFLAGS@ @CURL_CFLAGS@
test_client_LDADD = $(AM_LDFLAGS) @MICROHTTPD_LIBS@ @CURL_LIBS@ ../src/libhttptun.la
