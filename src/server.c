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
#define HC_OK "{\"status\": \"ok\"}\n"

#define RESOURCE_NOT_FOUND "{\"error\": \"Resource not found.\"}\n"

#define POST_BUFFER_SZ 32*1024

struct post_state_s {
    int fd;
    int eok;
};

typedef struct post_state_s post_state_t;

static int post_iterator (void *cls, enum MHD_ValueKind kind, const char *key, const char *filename, const char *content_type,
                          const char *transfer_encoding, const char *value, uint64_t off, size_t size) {
    post_state_t *post_state = cls;
    log_info("server", "iterator: post state address %p and fd is: %d", cls, post_state->fd);
    ssize_t written = 0;
    log_info("server", "Got key: %s and filename: %s (content-type: %s) and bytes: %zd (will write to fd: %d)", key, filename, content_type, size, post_state->fd);
    if (strcmp(key, "pkt") == 0) {
        while((size - written) > 0) {
            written += write(post_state->fd, value + off + written, size - written);
            log_warn("server", "Wrote %zd bytes to tunnel so far", written);
            sleep(1);
        }
        post_state->eok = 1;
    }
    return MHD_YES;
}

static int do_handle(void *cls, struct MHD_Connection *connection, const char *url, const char *method, const char *version,
                     const char *upload_data, size_t *upload_data_size, void **unused) {
    static int eok;
    const char * page;
    struct MHD_Response * response = NULL;
    struct MHD_PostProcessor *pp = NULL;
    unsigned int response_code;
    static post_state_t post_state;
    int ret;
    post_state.fd = *(int*) cls;
    char buff[POST_BUFFER_SZ];

    log_info("server", "fd : %d", post_state.fd);

    if (strcmp(method, "POST") == 0) {
        pp = *unused;
        if (pp == NULL) {
            post_state.eok = 0;
            log_info("server", "post state address %p", &post_state);
            pp = MHD_create_post_processor(connection, POST_BUFFER_SZ, &post_iterator, &post_state);
            *unused = pp;
        }

        MHD_post_process(pp, upload_data, *upload_data_size);

        if ((post_state.eok == 1) && (0 == *upload_data_size)) {
            ssize_t nr = 0;
            while (nr <= 0) {
                nr = read(post_state.fd, buff, POST_BUFFER_SZ);
                if (errno != EAGAIN) break;
                else log_info("server", "Read %zd bytes back from the tunnel", nr);
                usleep(100000);
            }
            log_info("server", "Responding with  %zd bytes back from the tunnel", nr);
            response = MHD_create_response_from_buffer(nr, (void*) buff, MHD_RESPMEM_MUST_COPY);
            response_code = MHD_HTTP_OK;
            *unused = NULL;
        } else {
            *upload_data_size = 0;
            return MHD_YES;
        }
    } else if (strcmp(method, "GET") == 0) {
        if (&eok != *unused) {
            *unused = &eok;
            return MHD_YES;            
        }

        if (0 != *upload_data_size) {
            log_warnx("server", "Got GET request %s with data, ignoring", url);
            return MHD_NO;
        }
         
        *unused = NULL;
        if (strcmp(url, "/hc") == 0) {
            page = HC_OK;
            response_code = MHD_HTTP_OK;
        } else {
            page = RESOURCE_NOT_FOUND;
            response_code = MHD_HTTP_NOT_FOUND;
        }
        response = MHD_create_response_from_buffer(strlen(page), (void*) page, MHD_RESPMEM_PERSISTENT);
    } else {
        log_warnx("server", "Unsupported verb: %s (%s)", method, url);
        return MHD_NO;
    }

    if (response == NULL) {
        return MHD_YES;
    } else {
        ret = MHD_queue_response(connection, response_code, response);
        MHD_destroy_response(response);
        if (pp != NULL) MHD_destroy_post_processor(pp);
        return ret;
    }
}

void run_server(int port, int tun_fd) {
    struct MHD_Daemon *d;
    log_warnx("server", "Starting HTTP server on port %d!", port);
    d = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, port, NULL, NULL, &do_handle, &tun_fd, MHD_OPTION_END);
    assert(d != NULL);
    while(! do_stop) sleep(1);
    fatal("server", "Stop requested");
    MHD_stop_daemon(d);
}
