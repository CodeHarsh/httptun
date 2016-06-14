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
 
#define BUFF_SZ (32 * 1024)
#define MAX_HOST_LEN 255
#define URL_SZ (MAX_HOST_LEN + 32)
#define PATH_FRAG "/"

static void make_pkt_post_url(const char *host, int port, char *buff, int buff_sz) {
    snprintf(buff, buff_sz, "http://%s:%d/pkt", host, port);
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

void run_client(const char *host, int port, int tun_fd) {
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

    if (MAX_HOST_LEN < strlen(host)) {
        log_crit("client", "host-name too long");
        return;
    }
    make_pkt_post_url(host, port, url, URL_SZ);
    curl = curl_easy_init();
    assert(curl != NULL);
    while(! do_stop) {
        read_len = read(tun_fd, buff, BUFF_SZ);
        if (read_len > 0) {
            curl_easy_reset(curl);
            post = last_post = NULL;
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_formadd(&post, &last_post,
                         CURLFORM_COPYNAME, "pkt",
                         CURLFORM_BUFFER, "pkt",
                         CURLFORM_BUFFERPTR, buff,
                         CURLFORM_BUFFERLENGTH, read_len,
                         CURLFORM_END);
            curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&tun_fd);
            res = curl_easy_perform(curl);

            if(res != CURLE_OK) {
                log_warnx("client", "curl_easy_perform() failed: %s", curl_easy_strerror(res));
            } else {
                curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed_upload);
                curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
                log_info("client", "Speed: %.3f bytes/sec during %.3f seconds\n", speed_upload, total_time);
            }   
        }
        log_info("client", "Let it go.. Let it go...!");
        usleep(1000);//1ms wait, so if something has to come back really fast, we catch it.
    }
    curl_easy_cleanup(curl);
}
