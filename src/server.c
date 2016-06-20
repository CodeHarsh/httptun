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

#define POST_BUFFER_SZ 64*1024

struct request_s {
    char buff[POST_BUFFER_SZ];
    struct MHD_PostProcessor *pp;
    int fd;
};

struct server_ctx_s {
    const char *username;
    const char *password;
    int fd;
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
    }
    return MHD_YES;
}

static int render_response(void *data,
                           int data_len,
                           const char *mime,
                           enum MHD_ResponseMemoryMode mem_mode,
                           int status_code,
                           struct MHD_Connection *connection) {
    
    struct MHD_Response *response = MHD_create_response_from_buffer(data_len, data, mem_mode);
    if (NULL == response)
        return MHD_NO;
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_ENCODING, mime);
    int ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

static int render_static_response_and_free_req(struct request_s *req,
                                               const char *data,
                                               const char *mime,
                                               int status_code,
                                               struct MHD_Connection *connection) {
    free(req);
    return render_response((void *)data, strlen(data), mime, MHD_RESPMEM_PERSISTENT, status_code, connection);
}

static int pkt_response(struct request_s *req, struct MHD_Connection *connection) {
    if (NULL == req)
        return MHD_NO;

    int buffered_bytes = 0;
    int bytes_read = read(req->fd, req->buff, POST_BUFFER_SZ);
    
    log_debug("server", "Read %d bytes off tun", bytes_read);
    if (bytes_read > 0) buffered_bytes = bytes_read;
    else if (errno != EAGAIN) log_warn("server", "Tun read failed, it read %d bytes", bytes_read);

    return render_response((void *)req, buffered_bytes, "application/octet-stream", MHD_RESPMEM_MUST_FREE, MHD_HTTP_OK, connection);
}

static int health_check_response(struct request_s *req, struct MHD_Connection *connection) {
    return render_static_response_and_free_req(req, "{\"status\": \"green\"}", "text/json", MHD_HTTP_OK, connection);
}

static int page_not_found_response(struct request_s *req, struct MHD_Connection *connection) {
    return render_static_response_and_free_req(req, "Page not found.", "text/plain", MHD_HTTP_NOT_FOUND, connection);
}

static int unauthorized_response(struct request_s *req, struct MHD_Connection *connection) {
    return render_static_response_and_free_req(req, "Unauthorized.", "text/plain", MHD_HTTP_UNAUTHORIZED, connection);
}


typedef int (*page_handler_fn_t)(struct request_s *req, struct MHD_Connection *connection);

struct page_s {
    const char *url;
    page_handler_fn_t handler;
    int requires_auth;
};

static struct page_s pages[] = {
    { "/pkt", &pkt_response, 1},
    { "/hc", &health_check_response, 0},
    { "/auth_required", &unauthorized_response, 0},
    { NULL, &page_not_found_response, 1}
};

#define UNAUTHORIZED_HANDLER 2

#define SH "server"

static struct page_s *resolve_page(struct server_ctx_s *s_ctx, const char *method, const char *url, struct MHD_Connection *connection) {
    unsigned int i = 0;
    while ((pages[i].url != NULL) && (0 != strcmp(pages[i].url, url))) i++;
    struct page_s *pg = &pages[i];

    if (pg->requires_auth) {
        char *pass = NULL;
        char *user = MHD_basic_auth_get_username_password(connection, &pass);
        if ((user == NULL) ||
            (0 != strcmp(user, s_ctx->username)) ||
            (0 != strcmp(pass, s_ctx->password))) {
            log_info("server", "Unauthorized access with username %s and password %s action [%s] %s", user, pass, method, url);
            pg = &pages[UNAUTHORIZED_HANDLER];
        }
        free(user); free(pass);
    }
    return pg;
}

static int do_handle(void *s_ctx_,
                     struct MHD_Connection *connection,
                     const char *url,
                     const char *method,
                     const char *version,
                     const char *upload_data,
                     size_t *upload_data_size,
                     void **ptr) {
    struct request_s *request;
    int ret;
    struct server_ctx_s *s_ctx = (struct server_ctx_s *) s_ctx_;

    struct page_s *pg = resolve_page(s_ctx, method, url, connection);
    
    request = *ptr;
    if (NULL == request) {
        request = calloc(1, sizeof (struct request_s));
        if (NULL == request) {
            log_warn(SH, "calloc error");
            return MHD_NO;
        }
        request->fd = s_ctx->fd;
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
    if (0 == strcmp(method, MHD_HTTP_METHOD_POST)) {
        MHD_post_process(request->pp, upload_data, *upload_data_size);
        if (0 != *upload_data_size) {
            *upload_data_size = 0;
            return MHD_YES;
        }

        MHD_destroy_post_processor(request->pp);
        request->pp = NULL;
        method = MHD_HTTP_METHOD_GET; /* fake 'GET' */
    }

    if ((0 == strcmp(method, MHD_HTTP_METHOD_GET)) ||
        (0 == strcmp(method, MHD_HTTP_METHOD_HEAD))) {
        ret = pg->handler(request, connection);
        if (ret != MHD_YES)
            log_warnx(SH, "Failed to create page for '%s'", url);
        return ret;
    }
    return render_static_response_and_free_req(request, "Method not supported.", "text/plain", MHD_HTTP_METHOD_NOT_ACCEPTABLE, connection);
}

void run_server(int port, int tun_fd,
                const char *username, const char *password,
                const char *ssl_key, const char *ssl_cert) {
    int flags = fcntl(tun_fd, F_GETFL, 0);
    assert(fcntl(tun_fd, F_SETFL, flags | O_NONBLOCK) == 0);

    struct server_ctx_s s_ctx;
    s_ctx.fd = tun_fd;
    s_ctx.username = username;
    s_ctx.password = password;
    struct MHD_Daemon *d;
    log_warnx("server", "Starting HTTP server on port %d!", port);
    if (ssl_key == NULL)
        d = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, port,
                             NULL, NULL,
                             &do_handle, &s_ctx,
                             MHD_OPTION_END);
    else
        d = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY | MHD_USE_SSL, port,
                             NULL, NULL,
                             &do_handle, &s_ctx,
                             MHD_OPTION_HTTPS_MEM_KEY, ssl_key,
                             MHD_OPTION_HTTPS_MEM_CERT, ssl_cert,
                             MHD_OPTION_END);
    assert(d != NULL);
    while(! do_stop) sleep(1);
    fatal("server", "Stop requested");
    MHD_stop_daemon(d);
}
