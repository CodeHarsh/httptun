#include "server.h"
#include "log.h"

#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>

int do_stop;

#define HC_OK "{\"status\": \"ok\"}\n"

#define RESOURCE_NOT_FOUND "{\"error\": \"Resource not found.\"}\n"

#define POST_BUFFER_SZ 32*1024

static int post_iterator (void *cls, enum MHD_ValueKind kind, const char *key, const char *filename, const char *content_type,
                          const char *transfer_encoding, const char *value, uint64_t off, size_t size) {
    int *eok = cls;
    log_info("server", "Got key: %s and filename: %s (content-type: %s)", key, filename, content_type);
    if (strcmp(key, "pkt") == 0) {
        fprintf(stderr, "%s\n", value);
        *eok = 1;
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
    int ret;

    if (strcmp(method, "POST") == 0) {
        pp = *unused;
        if (pp == NULL) {
            eok = 0;
            pp = MHD_create_post_processor(connection, POST_BUFFER_SZ, &post_iterator, &eok);
            *unused = pp;
        }

        MHD_post_process(pp, upload_data, *upload_data_size);

        if ((eok == 1) && (0 == *upload_data_size)) {
            response = MHD_create_response_from_buffer(strlen (url), (void *) url, MHD_RESPMEM_MUST_COPY);
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

static void interrupted(int sig) {
    do_stop = 1;
}

static void stop_on_sigint() {
    struct sigaction sig;
    sig.sa_handler = &interrupted;
    sigemptyset(&sig.sa_mask);
    assert(sigaction (SIGINT, &sig, NULL) == 0);
}

void start_server(int port) {
    do_stop = 0;
    stop_on_sigint();
    struct MHD_Daemon *d;
    log_warnx("server", "Starting HTTP server on port %d!", port);
    d = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, port, NULL, NULL, &do_handle, NULL, MHD_OPTION_END);
    assert(d != NULL);
    while(! do_stop) sleep(1);
    log_crit("server", "Stop requested");
    MHD_stop_daemon(d);
}
