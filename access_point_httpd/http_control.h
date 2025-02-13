#ifndef HTTP_CONTROL_H_SENTRY
#define HTTP_CONTROL_H_SENTRY

#include "lwip/apps/httpd.h"
#include "pico/stdlib.h"
#include "runtime_settings.h"
#include "utility.h"
#include <lwip/arch.h>
#include <lwip/def.h>
#include <pico/cyw43_arch.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_POST_DATA_LEN 1024
#define MAX_URI_LEN 64

typedef void (*process_post_field_fn)(const char *key, const char *value);
/* Handler-functions required by LWIP HTTPD */
// POST handler: called when a new POST request begins
err_t httpd_post_begin(void *connection, const char *uri,
                       const char *http_request, uint16_t http_request_len,
                       int content_len, char *response_uri,
                       uint16_t response_uri_len, uint8_t *post_auto_wnd);
// POST data handler: called for each incoming chunk of POST data
err_t httpd_post_receive_data(void *connection, struct pbuf *p);

// POST finished handler: called when all POST data has been received
void httpd_post_finished(void *connection, char *response_uri,
                         uint16_t response_uri_len);
int my_httpd_run(tSSIHandler my_ssi_handler, const char *ssitags[],
                 uint8_t number_of_tags,
                 process_post_field_fn process_post_field,
                 volatile bool *store_settings_flag);

#endif // HTTP_CONTROL_H_SENTRY
