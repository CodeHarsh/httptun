#include "client.h"
#include "stop.h"
#include "log.h"

#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
 
#define BUFF_SZ (32 * 1024)
#define MAX_HOST_LEN 255
#define URL_SZ (MAX_HOST_LEN + 32)
#define PATH_FRAG "/"

static void make_pkt_post_url(const char *host, int port, int use_ssl, char *buff, int buff_sz) {
    snprintf(buff, buff_sz, "%s://%s:%d/pkt", (use_ssl ? "https": "http"), host, port);
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    int fd = * (int *) userp;
    size_t len = size * nmemb;
    int written = 0;
    size_t total_written = 0;
    while ((total_written < len) && (written >= 0)) {
        written = write(fd, contents, len);
        if (written < 0) {
            log_warn("curl", "write to tun failed, bytes written: %zd, downloaded-bytes: %zd", total_written, len);
        }
        total_written+=written;
    }
    return total_written;
}

#define MIN_BACKOFF_MICRO_SEC 10 /* 10 micro-sec, used when we have work going on */

void increase_backoff(int *usec) {
    if (*usec < 1000) { /* < iterate under 1 ms for some time to bring quickly arriving response pkts */
        *usec += 10;
    } else if (*usec < 300000) { /* < 300 ms (healthy internet class latency + BIG factor of safety) */
        *usec += 10000;
    } else if (*usec < 60000000) {/* < 1 minute (no one is doing anything anyway) */
        *usec *= 2;
    }
    log_debug("client", "New backoff value: %d", *usec);
}

void do_backoff(int usec, int tun_fd) {
    fd_set rfds;
    struct timeval tv;
    int retval;
    int sec;

    FD_ZERO(&rfds);
    FD_SET(tun_fd, &rfds);

    sec = tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;

    retval = select(tun_fd + 1, &rfds, NULL, NULL, &tv);

    if (retval == -1) {
        log_crit("client", "select failed, falling-back to sleep");
        sleep(sec);
    }
}

void run_client(const char *host, int port, int tun_fd, const char *username, const char *password, int use_ssl) {
    int flags = fcntl(tun_fd, F_GETFL, 0);
    assert(fcntl(tun_fd, F_SETFL, flags | O_NONBLOCK) == 0);

    char buff[BUFF_SZ];
    char url[URL_SZ];
    int read_len;

    CURL *curl;
    CURLcode res;
    double speed_upload, total_time;
    struct curl_httppost *post;
    struct curl_httppost *last_post;

    int backoff = 0;

    if (MAX_HOST_LEN < strlen(host)) {
        log_crit("client", "host-name too long");
        return;
    }
    make_pkt_post_url(host, port, use_ssl, url, URL_SZ);
    curl = curl_easy_init();
    assert(curl != NULL);
    while(! do_stop) {
        read_len = read(tun_fd, buff, BUFF_SZ);
        if (read_len == -1 && errno != EAGAIN)
            log_warn("client", "Failed to read from tun");

        curl_easy_reset(curl);
        post = last_post = NULL;
        curl_easy_setopt(curl, CURLOPT_URL, url);
        if (read_len > 0) {
            curl_formadd(&post, &last_post,
                         CURLFORM_COPYNAME, "pkt",
                         CURLFORM_BUFFER, "pkt",
                         CURLFORM_BUFFERPTR, buff,
                         CURLFORM_BUFFERLENGTH, read_len,
                         CURLFORM_END);
            curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
            log_debug("client", "Sending %d bytes of data in current request", read_len);
            backoff = MIN_BACKOFF_MICRO_SEC;
            log_debug("client", "Have reset backoff to %d", backoff);
        } else {
            log_debug("client", "Sending NO-data in current request");
            increase_backoff(&backoff);
        }
        if (trace_on()) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&tun_fd);
        curl_easy_setopt(curl, CURLOPT_USERNAME, username);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
        if (use_ssl) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            log_warnx("client", "curl_easy_perform() failed: %s", curl_easy_strerror(res));
        } else {
            curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed_upload);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
            log_info("client", "Speed: %.3f bytes/sec during %.3f seconds\n", speed_upload, total_time);
        }

        do_backoff(backoff, tun_fd);
    }
    curl_easy_cleanup(curl);
}
