#include "server.h"
#include "log.h"
#include "stop.h"

#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#define POST_BUFFER_SZ 32*1024

struct request_s {
    char buff[POST_BUFFER_SZ];
    struct MHD_PostProcessor *pp;
    int fd;
    int buff_filled_upto;
};

static int post_iterator(void *ctx,
                         enum MHD_ValueKind kind,
                         const char *key,
                         const char *filename,
                         const char *content_type,
                         const char *transfer_encoding,
                         const char *value,
                         uint64_t off,
                         size_t size) {
    struct request_s *r = (struct request_s *) ctx;
    ssize_t written = 0;
    log_debug("server", "Got key: %s and filename: %s (content-type: %s) and bytes: %zd (will write to fd: %d)", key, filename, content_type, size, r->fd);
    if (strcmp(key, "pkt") == 0) {
        while((size - written) > 0) {
            written += write(r->fd, value + off + written, size - written);
            log_debug("server", "Wrote %zd bytes to tunnel so far", written);
        }
        usleep(1000);//1 ms wait, so we take advantage of something returning really fast
        r->buff_filled_upto = read(r->fd, r->buff, POST_BUFFER_SZ);
    }
    return MHD_YES;
}

static int pkt_response(void *ctx, struct MHD_Connection *connection) {
    int ret;
    struct MHD_Response *response;

    if (NULL == ctx)
        return MHD_NO;

    struct request_s *r = (struct request_s*) ctx;

    response = MHD_create_response_from_buffer(r->buff_filled_upto, (void *) ctx, MHD_RESPMEM_MUST_FREE);
    if (NULL == response)
        return MHD_NO;
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_ENCODING, "application/octet-stream");
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

static int health_check_response(void *ctx, struct MHD_Connection *connection) {
    int ret;
    struct MHD_Response *response;

    const char *status_ok = "{\"status\": \"green\"}";

    response = MHD_create_response_from_buffer(strlen(status_ok), (void *) status_ok, MHD_RESPMEM_PERSISTENT);
    if (NULL == response)
        return MHD_NO;
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_ENCODING, "text/json");
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

#define NOT_FOUND_ERROR 

static int page_not_found_response(void *ctx, struct MHD_Connection *connection) {
    int ret;
    struct MHD_Response *response;

    const char *page_not_found = "No such page exists.";

    response = MHD_create_response_from_buffer(strlen(page_not_found), (void *) page_not_found, MHD_RESPMEM_PERSISTENT);
    if (NULL == response)
        return MHD_NO;
    ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_ENCODING, "text/plain");
    MHD_destroy_response(response);
    return ret;
}

typedef int (*page_handler_fn_t)(void *ctx, struct MHD_Connection *connection);

struct page_s {
    const char *url;
    page_handler_fn_t handler;
};

static struct page_s pages[] = {
    { "/pkt", &pkt_response},
    { "/hc", &health_check_response},
    { NULL, &page_not_found_response}
};

#define SH "server_handler"

static int do_handle(void *fd,
                     struct MHD_Connection *connection,
                     const char *url,
                     const char *method,
                     const char *version,
                     const char *upload_data,
                     size_t *upload_data_size,
                     void **ptr) {
    struct MHD_Response *response;
    struct request_s *request;
    int ret;
    unsigned int i;
    void *ctx = NULL;

    request = *ptr;
    if (NULL == request) {
        request = calloc (1, sizeof (struct request_s));
        if (NULL == request) {
            log_warn(SH, "calloc error");
            return MHD_NO;
        }
        request->fd = *(int *)fd;
        *ptr = request;
        if (0 == strcmp (method, MHD_HTTP_METHOD_POST)) {
            request->pp = MHD_create_post_processor(connection, 1024, &post_iterator, request);
            if (NULL == request->pp) {
                log_warn(SH, "Failed to setup post processor for '%s'", url);
                return MHD_NO;
            }
        }
        return MHD_YES;
    }
    if (0 == strcmp (method, MHD_HTTP_METHOD_POST)) {
        MHD_post_process (request->pp, upload_data, *upload_data_size);
        if (0 != *upload_data_size) {
            *upload_data_size = 0;
            return MHD_YES;
        }

        MHD_destroy_post_processor(request->pp);
        request->pp = NULL;
        method = MHD_HTTP_METHOD_GET; /* fake 'GET' */
        ctx = request;
    }

    if ((0 == strcmp(method, MHD_HTTP_METHOD_GET)) ||
        (0 == strcmp(method, MHD_HTTP_METHOD_HEAD))) {
        i=0;
        while ((pages[i].url != NULL) && (0 != strcmp(pages[i].url, url))) i++;
        ret = pages[i].handler(ctx, connection);
        if (ret != MHD_YES)
            log_warnx(SH, "Failed to create page for '%s'", url);
        return ret;
    }
    const char *bad_method = "Method not supported.";
    response = MHD_create_response_from_buffer(strlen(bad_method), (void *) bad_method, MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, 406, response);
    MHD_destroy_response(response);
    return ret;
}

void run_server(int port, int tun_fd) {
    int flags = fcntl(tun_fd, F_GETFL, 0);
    assert(fcntl(tun_fd, F_SETFL, flags | O_NONBLOCK) == 0);
    
    struct MHD_Daemon *d;
    log_warnx("server", "Starting HTTP server on port %d!", port);
    d = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, port, NULL, NULL, &do_handle, &tun_fd, MHD_OPTION_END);
    assert(d != NULL);
    while(! do_stop) sleep(1);
    fatal("server", "Stop requested");
    MHD_stop_daemon(d);
}
