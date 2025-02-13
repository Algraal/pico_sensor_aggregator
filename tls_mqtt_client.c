#include "tls_mqtt_client.h"
#include "crypto_consts.h"
#include "utility.h"

#include <hardware/irq.h>
#include <lwip/arch.h>
#include <lwip/err.h>
#include <lwip/ip4_addr.h>
#include <lwip/ip_addr.h>
#include <mbedtls/ssl.h>
#include <pico.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>

#include <lwip/dns.h>
#include <lwip/pbuf.h>
#include <lwip/tcp.h>

#include <lwip/altcp_tcp.h>
#include <lwip/altcp_tls.h>
#include <lwip/apps/mqtt.h>

#include <pico/time.h>
#include <pico/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* Topics to subscribe */
const char topics[][116] = {PICO_HOSTNAME "/control/water",
                            PICO_HOSTNAME "/control/light"};
#define TLS_MQTT_NUMBER_OF_TOPICS sizeof(topics) / sizeof(topics[0])

/* MQTT_CLIENT_T static pointer to access mqtt client from the callbacks */
static MQTT_CLIENT_T *static_client;

// First define subscribe topics, then publish
TopicState topics_sessions[] = {
    {.topic_name = topics[0],
     .topic_number = 0,
     .topic_buffer = {0},
     .topic_buffer_length = 0,
     .is_subscribed = false,
     .data_in = 0},
    {.topic_name = topics[1],
     .topic_number = 1,
     .topic_buffer = {0},
     .topic_buffer_length = 0,
     .is_subscribed = false,
     .data_in = 0},
};

// Map error to human readable string. Will be used in logging and AP mode
// for runtime settings
const char *const tls_mqtt_error_messages[] = {
    "Success",
    "Error allocating dynamic memory",
    "Error retrieving server's IP during DNS lookup",
    "Error parsing certificates",
    "Error establishing connection",
    "Error uninitialized state",
    // mqtt_connect
    "Connection refused: wrong protocol version",
    "Connection refused: wrong ID",
    "Connection refused: server",
    "Connection refused: wrong credentials",
    "Connection refused: not authorized",
    "Disconnected - common error",
    "Disconnected - timeout",
    // topic errors. If error from this block appeared find the number of
    // topic with an error in topic_incom_data
    "Subscription topic is undefined",
    "Message from the broker exceeded buffer length",
    "Null terminator is absent",
    "Error publishing message",
    // TLS MQTT client requires reconfigration or updated info (e.g. certs,
    // passes etc)
    "Certs should be reconfigured",
    "Client information should be reconfigured",

};

// Maps enum value to human readable string
const char *tls_mqtt_strerr(TLS_MQTT_RET status) {
  return tls_mqtt_error_messages[status];
}

/**
 * @brief Called when client has connected to the server after mqtt_connect,
 * or when connection is closed by server or error appeared
 *
 * @param client MQTT client - required by the prototype
 * @param arg - Provided in tls_mqtt_connect arg, in this case a wrapper
 * MQTT_CLIENT_T
 * @param status - Connect result code/Disconnection notification
 */
static void tls_mqtt_connection_cb(mqtt_client_t *client, void *arg,
                                   mqtt_connection_status_t status) {
  MQTT_CLIENT_T *state = (MQTT_CLIENT_T *)arg;
  DEBUG_PRINT("connection_cb: mqtt_connection_status_t %d\n", status);
  switch (status) {
  /* Can proceed our MQTT communication */
  case MQTT_CONNECT_ACCEPTED:
    DEBUG_PRINT("connection to %s established\n",
                ipaddr_ntoa(&state->remote_addr));
    // Subscribe to the control topics
    state->is_connected = true;
    tls_mqtt_sub_unsub_topics(state, true);
    break;
  /* Two cases that can be resolved by reconnect. Note! Both of this cases can
   * appear due to the bugs and misconfiguration of the TLS that will lead to
   * endless reconfiguration.
   */
  case MQTT_CONNECT_DISCONNECTED:
    state->is_connected = false;
    tls_mqtt_connect(state);
    break;
  case MQTT_CONNECT_TIMEOUT:
    state->is_connected = false;
    tls_mqtt_connect(state);
    break;
  /* Other error statuses such as wrong protocol, wrong certs cannot be
   * resolved during the runtime */
  default:
    state->is_connected = false;
    break;
  }
}

static void tls_mqtt_incoming_publish_cb(void *arg, const char *topic,
                                         u32_t tot_len) {
  MQTT_CLIENT_T *state = arg;
  TopicState *session = state->topics_states;
  int i;
  state->topic_incom_data = TLS_MQTT_NUMBER_OF_TOPICS;

  for (i = 0; i < TLS_MQTT_NUMBER_OF_TOPICS; i++) {
    if (!strcmp(session[i].topic_name, topic)) {
      DEBUG_PRINT("tls_mqtt_pub_start_cb:\nSession name: %s\ntopic name %s\n",
                  session[i].topic_name, topic);
      state->topic_incom_data = i;
      state->err_state = TLS_MQTT_OK;
      break;
    }
  }
  // tot_len is the total length of the data to be received, it should be
  // processed now.
  if (state->topic_incom_data == TLS_MQTT_NUMBER_OF_TOPICS) {
    state->err_state = TOPIC_ERR_MQTT_UNDEFINED;
    return;
  }
  // Assign session receiving the incoming message
  session = &session[state->topic_incom_data];
  // Drop the unfinished message to the topic
  session->topic_buffer_length = 0;
  session->data_in = 0;
  // Check if the message will fit into the buffer. -1 for null terminator
  if (tot_len > sizeof(session->topic_buffer) - 1) {
    // Based on this state topic in incoming data callback should abort
    // writing
    state->err_state = TOPIC_ERR_MQTT_EXCEDEED_LENGTH;
  } else {
    session->data_in = tot_len;
    state->err_state = TLS_MQTT_OK;
  }
}

static void tls_mqtt_incoming_data_cb(void *arg, const uint8_t *data,
                                      uint16_t len, uint8_t flags) {
  MQTT_CLIENT_T *state = (MQTT_CLIENT_T *)arg;
  TopicState *session;

  if (state->topic_incom_data < TLS_MQTT_NUMBER_OF_TOPICS) {
    session = &state->topics_states[state->topic_incom_data];
  }
  TLS_MQTT_RET ret = state->err_state;
  switch (ret) {
  case TOPIC_ERR_MQTT_UNDEFINED:
    DEBUG_PRINT("error tls_mqtt_pub_data_cb: %s\n",
                tls_mqtt_strerr(state->err_state));
    break;
  case TOPIC_ERR_MQTT_EXCEDEED_LENGTH:
    DEBUG_PRINT("error tls_mqtt_pub_data_cb:\n\"%s\" %s\n", session->topic_name,
                tls_mqtt_strerr(state->err_state));
    break;
  case TLS_MQTT_OK:
    if (session->data_in > 0) {
      state->err_state = TLS_MQTT_OK;
      session->data_in -= len;
      memcpy(&session->topic_buffer[session->topic_buffer_length], data, len);
      session->topic_buffer_length += len;
      // MQTT broker does not add null terminator in the message (if binary data
      // is expected adding null terminator here and considering in
      // tls_mqtt_incoming_publish_cb should be removed)
      session->topic_buffer[session->topic_buffer_length] = 0;
      if (session->data_in == 0) {
        // Last portion of data sets this flag to true
        if (flags & MQTT_DATA_FLAG_LAST) {
          // Call function to process the server's command
          state->pass_incom_data(session->topic_number, session->topic_buffer,
                                 session->topic_buffer_length);
          DEBUG_PRINT("\nReceived message on topic \"%s\"\n%s\n",
                      session->topic_name, session->topic_buffer);
        } else {
          DEBUG_PRINT(
              "error tls_mqtt_pub_data_cb Last data portion without flag\n");
        }
        // Data processed, clean buffer
        session->topic_buffer_length = 0;
        session->topic_buffer[0] = 0;
      }
    } // Else nothing to be done...
    break;
  default:
    DEBUG_PRINT("error tls_mqtt_pub_data_cb:\n\"%s\" %s\n", session->topic_name,
                tls_mqtt_strerr(state->err_state));
    break;
  }
}
/* Called on publish request is completed. Errors are not handled, if message is
 lost it is ok
 */
static void clean_message(MQTTMessage *message) {
  if (message) {
    // Free individual fields
    if (message->topic) {
      free(message->topic);
      message->topic = NULL;
    }
    if (message->payload) {
      free(message->payload);
      message->payload = NULL;
    }
    // Free the message itself
    free(message);
  }
}
// Called on publish success, short on memory, and client disconnected. Because
// sensor data is not that valuable just clean the memory without resending
static void tls_mqtt_pub_request_cb(void *arg, err_t err) {
  MQTTMessage *message = (MQTTMessage *)arg;
  // Ensure pointer is valid before accessing
  if (message) {
    DEBUG_PRINT("tls_mqtt_pub_request_cb loop\n");
    DEBUG_PRINT("Message topic: %s\nMessage data: %s\nStatus: %d\n",
                message->topic ? message->topic : "NULL",
                (const char *)message->payload ? (const char *)message->payload
                                               : "NULL",
                err);

    // Clean up memory
    clean_message(message);

  } else {
    DEBUG_PRINT("Invalid or already cleaned message pointer.\n");
  }
}

static void tls_mqtt_subscribe_request_cb(void *arg, err_t err) {
  TopicState *session = (TopicState *)arg;
  if (err == ERR_OK) {
    // If session was unsubscribed now it is subscribed (sub case), for unsub
    // case reverted logic is applied
    session->is_subscribed = !session->is_subscribed;
  } else {
    /* Try to repeat failed sub/unsub */
    err = mqtt_sub_unsub(static_client->mqtt_client, session->topic_name, QOS,
                         tls_mqtt_subscribe_request_cb, session,
                         !session->is_subscribed);
    if (err != ERR_OK) {
      DEBUG_PRINT("tls_mqtt_subscribe_request_cb() error: %d\n", err);
    }
  }
}

/* MQTT_CLIENT_T configuration */
#if ENABLE_TLS

// Inits tls config, if config exists recreates
TLS_MQTT_RET tls_mqtt_reconfigure_tls_config(MQTT_CLIENT_T *state,
                                             const uint8_t *ca_cert,
                                             const uint8_t *client_key,
                                             const uint8_t *client_cert) {
  DEBUG_PRINT("Entered tls_mqtt_reconfigure_tls_config\n");
  if (state->tls_config != NULL) {
    cyw43_arch_lwip_begin();
    altcp_tls_free_config(state->tls_config);
    cyw43_arch_lwip_end();

    state->tls_config = NULL;
  }
  cyw43_arch_lwip_begin();
  state->tls_config = altcp_tls_create_config_client_2wayauth(
      ca_cert, strlen((const char *)ca_cert) + 1, client_key,
      strlen((const char *)client_key) + 1, (const uint8_t *)"", 0, client_cert,
      strlen((const char *)client_cert) + 1);
  cyw43_arch_lwip_end();
  if (state->tls_config == NULL) {
    DEBUG_PRINT("error tls_mqtt_reconfigure_tls_config\n");
    state->err_state = TLS_MQTT_ERR_CERTS;
    return TLS_MQTT_ERR_CERTS;
  }
  DEBUG_PRINT("tls_mqtt_reconfigure_tls_config successfully finished\n");
  state->err_state = TLS_MQTT_OK;
  return TLS_MQTT_OK;
}
#endif //  !ENABLE_TLS

// Subs if true, unsubs if false all the subscription-based topics.
void tls_mqtt_sub_unsub_topics(MQTT_CLIENT_T *client, bool sub) {
  TopicState *session;
  err_t err;
  for (int i = 0; i < TLS_MQTT_NUMBER_OF_TOPICS; i++) {
    session = &client->topics_states[i];
    // Perform sub/unsub only if the state should changed
    if (sub != session->is_subscribed) {
      cyw43_arch_lwip_begin();
      err = mqtt_sub_unsub(client->mqtt_client, session->topic_name, QOS,
                           tls_mqtt_subscribe_request_cb, session, sub);
      cyw43_arch_lwip_end();
      if (err != ERR_OK) {
        DEBUG_PRINT("tls_mqtt_sub_unsub_topics() error: %d\n", err);
      }
    }
  }
}
err_t tls_mqtt_publish(MQTT_CLIENT_T *client, const char *topic,
                       const uint8_t *payload, uint16_t payload_size,
                       uint8_t qos) {
  if (!client->is_connected) {
    DEBUG_PRINT("tls_mqtt_publish client is disconnected\n");
    return ERR_OK;
  }
  if (topic == NULL || payload == NULL || payload_size == 0) {
    DEBUG_PRINT("tls_mqtt_publish empty message provided\n");
    return ERR_OK;
  }
  uint16_t size;
  // Allocate memory a for new message
  MQTTMessage *new_message = (MQTTMessage *)malloc(sizeof(MQTTMessage));
  if (new_message == NULL) {
    return ERR_MEM;
  }
  new_message->topic = NULL;
  new_message->payload = NULL;
  new_message->payload_length = 0;
  // Copy topic
  size = strlen(topic);
  new_message->topic = (char *)malloc(size + 1);
  if (new_message->topic == NULL) {
    clean_message(new_message);
    return ERR_MEM;
  }
  strcpy(new_message->topic, topic);
  new_message->topic[size] = 0;
  // Copy payload
  new_message->payload = (uint8_t *)malloc(payload_size);
  if (new_message->payload == NULL) {
    clean_message(new_message);
    return ERR_MEM;
  }
  memcpy(new_message->payload, payload, payload_size);
  new_message->payload_length = payload_size;
  uint8_t retain = 0;
  cyw43_arch_lwip_begin();
  err_t err = mqtt_publish(client->mqtt_client, new_message->topic,
                           new_message->payload, new_message->payload_length,
                           qos, retain, tls_mqtt_pub_request_cb, new_message);
  cyw43_arch_lwip_end();
  DEBUG_PRINT("Message to be sent:\nTopic: %s\nText: %s\nLength: %d\n",
              new_message->topic, new_message->payload,
              new_message->payload_length);
  return err;
}

// Unsub sessions, disconnect, clean
void tls_mqtt_deinit(MQTT_CLIENT_T **client_ptr) {
  MQTT_CLIENT_T *client = *client_ptr;
  if (client == NULL) {
    return;
  }
  cyw43_arch_lwip_begin();
  bool ret = mqtt_client_is_connected(client->mqtt_client);
  cyw43_arch_lwip_end();
  if (ret) {
    tls_mqtt_sub_unsub_topics(client, false);
  }
  // mqtt disconnect forces pending publish requests to be called with ERR_CONN
  // so the messages are cleaned from the callback
  cyw43_arch_lwip_begin();
  mqtt_disconnect(client->mqtt_client);
  cyw43_arch_lwip_end();
  // unsub all of the clients
  // disconnect the clinet
  tls_mqtt_clean(client_ptr);
}

err_t tls_mqtt_connect(MQTT_CLIENT_T *state) {
  err_t err;
  cyw43_arch_lwip_begin();
  unsigned long port = strtoul(state->settings->tls_mqtt_broker_port, NULL, 10);
  err = mqtt_client_connect(state->mqtt_client, &(state->remote_addr), port,
                            tls_mqtt_connection_cb, state, state->ci);
  cyw43_arch_lwip_end();

  if (err != ERR_OK) {
    DEBUG_PRINT("mqtt_connect returned %d\n", err);
    return err;
  }
  state->err_state = TLS_MQTT_OK;
  cyw43_arch_lwip_begin();
  mqtt_set_inpub_callback(state->mqtt_client, tls_mqtt_incoming_publish_cb,
                          tls_mqtt_incoming_data_cb, state);
  cyw43_arch_lwip_end();
  return err;
}
// MQTT related
void tls_mqtt_clean(MQTT_CLIENT_T **client_ptr) {
  int i;
  MQTT_CLIENT_T *client = *client_ptr;
  // Zero sessions
  for (i = 0; i < sizeof(topics_sessions) / sizeof(topics_sessions[0]); i++) {
    client->topics_states[i].topic_buffer[0] = 0;
    client->topics_states[i].is_subscribed = 0;
    client->topics_states[i].topic_buffer_length = 0;
    client->topics_states[i].data_in = 0;
  }
  // Free settings
  free(client->settings);
  client->settings = NULL;
  // Free mqtt client
  cyw43_arch_lwip_begin();
  mqtt_client_free(client->mqtt_client);
  cyw43_arch_lwip_end();
  client->mqtt_client = NULL;
  // Free client configuration
  free(client->ci);
  client->ci = NULL;

#if ENABLE_TLS
  // Free TLS config
  cyw43_arch_lwip_begin();
  altcp_tls_free_config(client->tls_config);
  cyw43_arch_lwip_end();
  client->tls_config = NULL;
#endif // !ENABLE_TLS

  free(client);
  *client_ptr = NULL;
  static_client = NULL;
  DEBUG_PRINT("MQTT client is cleaned up\n");
}

static TLS_MQTT_RET tls_mqtt_reconfigure_client(MQTT_CLIENT_T *state) {
  DEBUG_PRINT("entered tls_mqtt_reconfigure_client\n");
  if (state->ci != NULL) {
    DEBUG_PRINT("ci in state non NULL\n");
    free(state->ci);
  }
  struct mqtt_connect_client_info_t *ci =
      malloc(sizeof(struct mqtt_connect_client_info_t));
  if (ci == NULL) {
    DEBUG_PRINT("tls_mqtt_reconfigure_client error allocating memory\n");
    return TLS_MQTT_ERR_ALLOC;
  }
  memset(ci, 0, sizeof(struct mqtt_connect_client_info_t));
  DEBUG_PRINT("Start configuration\n");
  ci->client_id = state->settings->tls_mqtt_client_id;
  ci->client_user = (state->settings->tls_mqtt_client_name[0] == 0
                         ? NULL
                         : state->settings->tls_mqtt_client_name);
  ci->client_pass = (state->settings->tls_mqtt_client_password[0] == 0
                         ? NULL
                         : state->settings->tls_mqtt_client_password);
  /*The Keep Alive interval is set when a client connects to the broker, in
   * the CONNECT packet. If the Keep Alive interval is set to 60 seconds,
   * the client must send any MQTT control packet (e.g., a PINGREQ , PUBLISH
   * , SUBSCRIBE , etc.) within 90 seconds (60 * 1.5) to inform the broker
   * that it's still connected. */
  ci->keep_alive = 300;
  /* Last Will and Testament (LWT) are used to notify other MQTT
   * clients about the unexpected disconnection of this client */
  ci->will_topic = NULL;
  ci->will_msg = NULL;
  ci->will_retain = 0;
  ci->will_qos = 0;
#if ENABLE_TLS
  // !REMARK field before is added to lwip/apps/mqtt.h as a git patch, it is
  // required for SNI. Check the patch file in the project directory
  ci->server_name = state->settings->tls_mqtt_broker_CN;
  // security config
  ci->tls_config = state->tls_config;
#endif // !ENABLE_TLS
  state->ci = ci;
  return TLS_MQTT_OK;
}

// Initialize MQTT client
TLS_MQTT_RET
tls_mqtt_init(MQTT_CLIENT_T **client_ptr, tls_mqtt_settings *settings,
              data_handler_fn process_command) {
  int i;
  TLS_MQTT_RET ret;
  MQTT_CLIENT_T *client;
  client = malloc(sizeof(MQTT_CLIENT_T));
  if (!client) {
    ret = TLS_MQTT_ERR_ALLOC;
    DEBUG_PRINT("error tls_mqtt_init(): %s\n", tls_mqtt_strerr(ret));
    return ret;
  }
  static_client = client;
  client->settings = settings;
  client->err_state = TLS_MQTT_OK;
  client->topics_states = topics_sessions;
  client->is_connected = 0;
  client->ci = NULL;
#if ENABLE_TLS
  client->tls_config = NULL;
#endif // ENABLE_TLS
  client->topic_incom_data = TLS_MQTT_NUMBER_OF_TOPICS;
  // Sets the function responsible for executing server command
  client->pass_incom_data = process_command;
  // Sets the function to fetch data to be published
  // Zeros addr structure
  memset(&client->remote_addr, 0, sizeof(ip_addr_t));
  // Create a TLS configuration
  client->mqtt_client = mqtt_client_new();
  if (client->mqtt_client == NULL) {
    ret = TLS_MQTT_ERR_ALLOC;
    DEBUG_PRINT("error tls_mqtt_init(): %s\n", tls_mqtt_strerr(ret));
    tls_mqtt_clean(&client);
    return ret;
  }
  client->err_state = TLS_MQTT_OK;
  // Query hostname to resolve an IP address. Works as well for string with
  // IP addresses
  run_dns_lookup(client, settings->tls_mqtt_broker_hostname);
  // Non-fatal error, just ask user for another hostname
  if (client->err_state != TLS_MQTT_OK) {
    ret = client->err_state;
    tls_mqtt_clean(&client);
    return ret;
  }
  DEBUG_PRINT("DNS query finished with resolved addr: %s\n",
              ipaddr_ntoa(&client->remote_addr));

#if ENABLE_TLS
  ret = tls_mqtt_reconfigure_tls_config(client, (const uint8_t *)CA_CERT,
                                        (const uint8_t *)CLIENT_KEY,
                                        (const uint8_t *)CLIENT_CERT);
  if (ret != TLS_MQTT_OK) {
    tls_mqtt_clean(&client);
    return ret;
  }
#endif // !ENABLE_TLS

  // Client configuration
  ret = tls_mqtt_reconfigure_client(client);
  if (ret != TLS_MQTT_OK) {
    client->err_state = ret;
    tls_mqtt_clean(&client);
    return ret;
  }
  // Proper rewriting of the pointer outside the function scope
  *client_ptr = client;
  return TLS_MQTT_OK;
}

/*DNS*/
static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
  MQTT_CLIENT_T *state = (MQTT_CLIENT_T *)arg;
  if (ipaddr == NULL) {
    state->err_state = TLS_MQTT_ERR_DNS;
    DEBUG_PRINT("DNS error resolving addr\n");
    return;
  }
  DEBUG_PRINT("DNS query finished with resolved addr: %s\n",
              ipaddr_ntoa(ipaddr));
  state->remote_addr = *ipaddr;
  state->err_state = TLS_MQTT_OK;
}

// Both IP and Domain are processed the same way
void run_dns_lookup(MQTT_CLIENT_T *state, const char *host) {
  err_t err;
  DEBUG_PRINT("Running DNS query for %s.\n", host);
  cyw43_arch_lwip_begin();
  // The callback based function (raw api)
  err = dns_gethostbyname(host, &(state->remote_addr), dns_found_cb, state);
  cyw43_arch_lwip_end();
  switch (err) {
  case ERR_OK:
    DEBUG_PRINT("DNS query query not needed\n");
    break;
  case ERR_ARG:
    DEBUG_PRINT("DNS query argument error provded\n");
    break;
  }
  while (state->remote_addr.addr == 0 && state->err_state != TLS_MQTT_ERR_DNS) {
#ifdef PICO_CYW43_ARCH_POLL
    cyw43_arch_poll();
#endif // PICO_CYW43_ARCH_POLL
    sleep_ms(1);
  }
}
