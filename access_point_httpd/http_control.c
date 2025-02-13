#include "http_control.h"
#include "utility.h"

#include <lwip/apps/httpd.h>
#include <lwip/def.h>

// Buffer to store incoming POST data
static char post_data_buffer[MAX_POST_DATA_LEN];
static uint16_t post_data_index = 0;
static process_post_field_fn process_post_field_cb;
static volatile bool *static_store_settings_flag = NULL;

// POST handler: called when a new POST request begins
err_t httpd_post_begin(void *connection, const char *uri,
                       const char *http_request, uint16_t http_request_len,
                       int content_len, char *response_uri,
                       uint16_t response_uri_len, uint8_t *post_auto_wnd) {
  DEBUG_PRINT("POST request received for URI: %s\n", uri);

  // Check content length
  if (content_len > MAX_POST_DATA_LEN) {
    DEBUG_PRINT("POST content too large: %d bytes\n", content_len);
    strncpy(response_uri, "/index.ssi",
            response_uri_len); // Point to error page
    return ERR_VAL;
  }

  // Reset buffer and set the response URI
  post_data_index = 0;
  return ERR_OK;
}

// POST data handler: called for each incoming chunk of POST data
err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
  if (!p)
    return ERR_ARG; // Ensure pbuf is valid

  char *data = (char *)p->payload;
  uint16_t len = p->len;

  // Ensure buffer won't overflow
  if (post_data_index + len > MAX_POST_DATA_LEN) {
    DEBUG_PRINT("POST data buffer overflow!\n");
    pbuf_free(p);
    return ERR_MEM;
  }

  // Append incoming data to the buffer
  memcpy(&post_data_buffer[post_data_index], data, len);
  post_data_index += len;

  pbuf_free(p); // Free the pbuf after processing
  return ERR_OK;
}

static void parse_buffer() {
  uint16_t index_begin = 0, index_end = 0;
  // Key-value are separted via "=", pairs are separted via "&"
  char *token = strtok(post_data_buffer, "&");
  char *key_value_sep;
  while (token != NULL) {
    // Loop to the end of the token, looking for "="
    key_value_sep = token;
    // Until null terminator set by strtok is met
    while (*key_value_sep != 0) {
      // HTTP changes "=" to %3D, so it will appear only as a control character
      if (*key_value_sep == '=') {
        // Set null term between key and value
        *key_value_sep = 0;
        if (*(key_value_sep + 1) != '0') {
          process_post_field_cb(token, key_value_sep + 1);
        } else {
          process_post_field_cb(token, "");
        }
        break;
      }
      key_value_sep++;
    }
    token = strtok(NULL, "&");
  }
}

// POST finished handler: called when all POST data has been received
void httpd_post_finished(void *connection, char *response_uri,
                         uint16_t response_uri_len) {
  DEBUG_PRINT("POST data received:\n%s\n", post_data_buffer);
  parse_buffer();
  *static_store_settings_flag = true;
  DEBUG_PRINT("POST processing complete. Sending response: %s\n", response_uri);

  memset(post_data_buffer, 0, sizeof(post_data_buffer));
  strncpy(response_uri, "/index.ssi", response_uri_len);
}

int my_httpd_run(tSSIHandler my_ssi_handler, const char *ssitags[],
                 uint8_t number_of_tags,
                 process_post_field_fn process_post_field,
                 volatile bool *store_settings_flag) {
  process_post_field_cb = process_post_field;
  static_store_settings_flag = store_settings_flag;
  cyw43_arch_lwip_begin();
  http_set_ssi_handler(my_ssi_handler, ssitags, number_of_tags);
  httpd_init();
  cyw43_arch_lwip_end();

  return 0;
}
