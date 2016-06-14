AM_CFLAGS = $(MORE_CFLAGS)
AM_CPPFLAGS = $(MORE_CPPFLAGS)
AM_LDFLAGS = $(MORE_LDFLAGS)

bin_PROGRAMS = httptun
dist_man_MANS = httptun.8

## TODO:5000 When you want to add more files, add them below.
## TODO:5000 Automake will find dependencies by itself. Run
## TODO:5000 ./autogen.sh after modifying this file.

noinst_LTLIBRARIES = libhttptun.la

libhttptun_la_SOURCES  = log.c log.h server.h server.c tun.h tun.c client.h client.c stop.h sig_handle.h sig_handle.c
libhttptun_la_CPPFLAGS = $(AM_CFLAGS) @MICROHTTPD_CFLAGS@ @CURL_CFLAGS@
libhttptun_la_LIBADD =  $(AM_LDFLAGS) @MICROHTTPD_LIBS@ @CURL_LIBS@

httptun_SOURCES  = httptun.h httptun.c
httptun_CFLAGS   = $(AM_CFLAGS) @MICROHTTPD_CFLAGS@ @CURL_CFLAGS@
httptun_LDFLAGS  = $(AM_LDFLAGS) @MICROHTTPD_LIBS@ @CURL_LIBS@
httptun_LDADD    = libhttptun.la

## TODO:2004 Each time you have used `PKG_CHECK_MODULES` macro
## TODO:2004 in `configure.ac`, you get two variables that
## TODO:2004 you can substitute like above.
