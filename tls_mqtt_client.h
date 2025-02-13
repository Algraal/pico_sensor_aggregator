#ifndef TLS_MQTT_CLIENT_SENTRY
#define TLS_MQTT_CLIENT_SENTRY

// Include secure information
#include "crypto_consts.h"
#include "runtime_settings.h"
#include "utility.h"

#include <hardware/regs/timer.h>
#include <lwip/apps/mqtt.h>
#include <lwip/apps/mqtt_opts.h>
#include <lwip/arch.h>
#include <lwip/err.h>
#include <lwip/ip_addr.h>
#include <pico/cyw43_arch.h>
#include <pico/time.h>
#include <pico/types.h>
#include <stdbool.h>
#include <stddef.h>
/**
 * @enum TLS_MQTT_RET
 * @brief Possible return values and error codes for the MQTT client.
 *
 * Describes the status or error codes returned by the MQTT client functions.
 * These cover a wide range of scenarios, including initialization, connection,
 * and data handling errors.
 *
 * @see tls_mqtt_strerr
 */
typedef enum {
  TLS_MQTT_OK = 0,                /**< Operation successful */
  TLS_MQTT_ERR_ALLOC,             /**< Memory allocation failed */
  TLS_MQTT_ERR_DNS,               /**< DNS resolution failed */
  TLS_MQTT_ERR_CERTS,             /**< Certificate configuration failed */
  TLS_MQTT_ERR_CONNECT,           /**< Connection error */
  TLS_MQTT_ERR_UNIT_STATE,        /**< Unit is in an invalid state */
  MQTT_REFUSED_PROTOCOL_VERSION,  /**< Unsupported MQTT protocol version */
  MQTT_REFUSED_IDENTIFIER,        /**< Client identifier rejected */
  MQTT_REFUSED_SERVER,            /**< Server unavailable */
  MQTT_REFUSED_USERNAME_PASS,     /**< Invalid username or password */
  MQTT_REFUSED_NOT_AUTHORIZED,    /**< Client is not authorized */
  MQTT_DISCONNECTED,              /**< Disconnected from MQTT server */
  MQTT_TIMEOUT,                   /**< Timeout during operation */
  TOPIC_ERR_MQTT_UNDEFINED,       /**< Undefined topic error */
  TOPIC_ERR_MQTT_EXCEDEED_LENGTH, /**< Topic buffer exceeded length */
  TOPIC_ERR_MQTT_NO_ZERO_TERM,    /**< Missing null terminator in topic */
  TLS_MQTT_PUBLISH_ERR,           /**< Error during publishing */
  TLS_MQTT_RECONF_CERTS, /**< Reconfiguration of certificates required */
  TLS_MQTT_RECONF_CLIENT /**< Client reconfiguration required */
} TLS_MQTT_RET;

/**
 * @brief Represents an MQTT message to be published or received.
 */
typedef struct {
  char *topic;             /**< Pointer to the topic name. */
  uint8_t *payload;        /**< Pointer to the payload data. */
  uint16_t payload_length; /**< Length of the payload data. */
} MQTTMessage;

/**
 * @brief Represents the state of a subscription topic.
 */
typedef struct {
  uint8_t topic_buffer[128];    /**< Buffer to store topic data */
  const uint8_t topic_number;   /**< ID of the topic used for fast access */
  const char *const topic_name; /**< Name of the topic */
  size_t topic_buffer_length;   /**< Current length of the topic */
  bool is_subscribed;           /**< Subscription status of the topic */
  size_t data_in; /**< Remaining incoming data length for the topic */
} TopicState;

/**
 * @brief Function prototype called from tls_mqtt_incoming_data_cb when the last
 * portion of data is received
 *
 * @param[in] topic_number The unique topic identifier
 * @param[in] data Pointer to the received data
 * @param[in] len Length of the received data
 * @pre should be defined and passed to tls_mqtt_init
 * @see tls_mqtt_incoming_data_cb
 */
typedef void (*data_handler_fn)(uint8_t topic_number, const uint8_t *data,
                                size_t len);

/**
 * @brief Function prototype called in tls_mqtt_run main loop to fetch new
 * messages to be sent
 *
 * Fetches data (user should define the source of the data) to be published on
 * a specific topic. The function should allocate space for the message and
 * populate the required parameters.
 *
 * @param[out] topic_name Pointer to store the topic name for publishing.
 * @param[out] data Pointer to store the payload data.
 * @param[out] len Pointer to store the length of the payload.
 *
 * @pre Must be defined and passed to `tls_mqtt_init`.
 *
 * @details The function is responsible for:
 * - Allocating space for the payload data and copying the message into it.
 * - Setting the `topic_name`, `data`, and `len` parameters accordingly.
 * - Freeing the space allocated for the original data, if necessary.
 *
 * The allocated data for the message will be freed by the corresponding
 * callback function after publishing.
 *
 * @see tls_mqtt_pub_request_cb
 */
typedef void (*fetch_to_be_published_fn)(char **topic_name, uint8_t **data,
                                         uint16_t *len);
/**
 * @brief Represents the state of the MQTT client
 */
typedef struct {
  struct mqtt_connect_client_info_t *ci; /**< MQTT connection info */
  ip_addr_t remote_addr;                 /**< Remote server address */
  mqtt_client_t *mqtt_client;            /**< mqtt_client instance */
#if ENABLE_TLS
  struct altcp_tls_config *tls_config; /**< TLS configuration */
#endif                                 // !ENABLE_TLS
  TopicState *topics_states;           /**< Array of topic states */
  TLS_MQTT_RET err_state;              /**< Current error state of the client */
  bool is_connected;                   /**< Connection status */
  int topic_incom_data; /**< Identifier of the topic receiving data*/
  data_handler_fn
      pass_incom_data; /**< Custom function to process income data */
  tls_mqtt_settings *settings;
} MQTT_CLIENT_T;

/**
 * @brief Converts TLS_MQTT_RET value to a string description
 *
 * @param[in] status That TLS_MQTT_RET value
 * @return A string description of the return value
 */
const char *tls_mqtt_strerr(TLS_MQTT_RET status);
/**
 * @brief Initializes and configures the MQTT client.
 *
 * @param[out] client Pointer to store the allocated MQTT client.
 * @param[in] process_command Callback to process received data.
 * @param[in] pass_incom_data Callback to fetch data for publishing.
 * @return TLS_MQTT_OK on success, or an appropriate error code.
 *
 * @details This function:
 * - Allocates memory for the MQTT client and initializes its fields.
 * - Sets up the MQTT connection (including TLS configuration).
 * - Performs DNS resolution for the specified MQTT server hostname.
 * - Sets all internal state to defaults.
 *
 * @pre The following macros must be defined before calling this function:
 *      - MQTT_HOSTNAME
 *      - CA_CERT
 *      - CLIENT_KEY
 *      - CLIENT_CERT
 *
 * @post On failure, `tls_mqtt_clean` is automatically called to release
 * allocated resources.
 *
 * @see run_dns_lookup, tls_mqtt_reconfigure_tls_config
 */
TLS_MQTT_RET tls_mqtt_init(MQTT_CLIENT_T **client, tls_mqtt_settings *settings,
                           data_handler_fn process_command);

/**
 * @brief Reconfigures the TLS configuration for the MQTT client.
 *
 * @param[in,out] state The MQTT client state.
 * @param[in] ca_cert The CA certificate.
 * @param[in] client_key The client's private key.
 * @param[in] client_cert The client's certificate.
 * @return TLS_MQTT_OK on success, or an appropriate error code.
 *
 * @details Frees any existing TLS configuration, then allocates and initializes
 * a new configuration using the provided certificates and keys.
 */
TLS_MQTT_RET tls_mqtt_reconfigure_tls_config(MQTT_CLIENT_T *state,
                                             const uint8_t *ca_cert,
                                             const uint8_t *client_key,
                                             const uint8_t *client_cert);

/**
 * @brief Cleans up the MQTT client state and resources.
 *
 * @param[in,out] client_ptr Pointer to the MQTT client.
 * @details Frees all memory allocated for the client and resets its state.
 */
void tls_mqtt_clean(MQTT_CLIENT_T **client_ptr);

/**
 * @brief Disconnects and deinitializes the MQTT client.
 *
 * @param[in,out] client_ptr Pointer to the MQTT client.
 * @details This function:
 * - Disconnects all subscribed topics.
 * - Disconnects from the MQTT server.
 * - Frees memory allocated for the client state.
 * - Resets static fields to their default values.
 *
 * @see tls_mqtt_clean
 */
void tls_mqtt_deinit(MQTT_CLIENT_T **client_ptr);

/**
 * @brief Publishes a message to an MQTT topic.
 *
 * This function publishes a message with the specified topic, payload, and QoS
 * to an MQTT client. The function handles memory allocation for the message
 * and ensures proper cleanup in case of errors. If the publication is
 * successful, the message is managed asynchronously by the MQTT library, and
 * its memory is freed in the associated callback (`tls_mqtt_pub_request_cb`).
 *
 * @param[in] client        Pointer to the MQTT client structure. Must not be
 * NULL.
 * @param[in] topic         The topic string to publish to. Must not be NULL.
 * @param[in] payload       Pointer to the message payload. Must not be NULL.
 * @param[in] payload_size  Size of the payload in bytes. Must be greater than
 * 0.
 * @param[in] qos           Quality of Service level for the message (0, 1, or
 * 2).
 *
 * @return
 * - `ERR_OK` on successful queuing of the message.
 * - `ERR_MEM` if memory allocation fails.
 * - `ERR_ARG` if invalid arguments are provided.
 * - Other error codes depending on the underlying MQTT publish function.
 *
 * @note
 * - The `topic` and `payload` strings are copied into dynamically allocated
 *   memory, so the caller does not need to manage their lifetime after this
 *   function is called.
 * - The callback `tls_mqtt_pub_request_cb` is invoked after the publish
 *   operation is completed to clean up the allocated memory.
 * - The function assumes the `client->mqtt_client` is properly initialized.
 *
 * @warning Ensure that the MQTT client is connected before calling this
 * function, or the publish may fail with an `ERR_CONN` error.
 *
 * @example
 * @code
 * MQTT_CLIENT_T my_client;
 * const char *topic = "example/topic";
 * const char *payload = "Hello, MQTT!";
 * uint16_t payload_size = strlen(payload);
 * uint8_t qos = 1;
 *
 * err_t result = tls_mqtt_publish(&my_client, topic, (const uint8_t *)payload,
 * payload_size, qos); if (result != ERR_OK) { printf("Failed to publish
 * message, error code: %d\n", result);
 * }
 * @endcode
 */

err_t tls_mqtt_publish(MQTT_CLIENT_T *client, const char *topic,
                       const uint8_t *payload, uint16_t payload_size,
                       uint8_t qos);
/**
 * @brief Subscribe or unsubscribe all subscription-based topics for the MQTT
 * client.
 *
 * This function iterates through all topics associated with the provided MQTT
 * client and performs a subscription or unsubscription operation based on the
 * `sub` parameter. The operation is skipped for topics that are already in the
 * desired state, ensuring efficient handling of the topic states.
 *
 * @param[in] client A pointer to the MQTT client structure.
 * @param[in] sub    A boolean indicating the desired operation:
 *                   - `true` to subscribe to topics.
 *                   - `false` to unsubscribe from topics.
 *
 * @note
 * - Errors for individual topics are logged using `DEBUG_PRINT` with the topic
 * name and error code.
 * - Each topic's subscription request result is stored in the `err_req` field
 * of its corresponding `TopicState` structure.
 * - This function operates on a "best-effort" basis; no rollback mechanism is
 * provided in case of partial failure.
 *
 * @warning The function does not return an aggregated result for the operation.
 * Ensure to check individual topic states if specific error handling is
 * required.
 *
 * Example usage:
 * @code
 * MQTT_CLIENT_T client;
 * // Initialize client and topics...
 * tls_mqtt_sub_unsub_topics(&client, true); // Subscribe to all topics
 * tls_mqtt_sub_unsub_topics(&client, false); // Unsubscribe from all topics
 * @endcode
 */
void tls_mqtt_sub_unsub_topics(MQTT_CLIENT_T *client, bool sub);
/**
 * @brief Establishes a TLS-secured MQTT connection to a remote broker.
 *
 * This function initiates a connection to an MQTT broker over TLS. It sets up
 the MQTT client with the
 * provided state, including callbacks for incoming publish messages and
 incoming data handling.
 *
 * @param[in] state A pointer to the MQTT client state structure. This structure
 must be properly initialized
 *                  before calling this function.
 *
 * @return
 *   - `ERR_OK` if the connection is successfully established.
 *   - An appropriate error code from the `err_t` enumeration if the connection
 fails.
 *
 * @note
 * - This function uses the `cyw43_arch_lwip` locking mechanism to ensure thread
 safety while performing
 *   network operations.
 * - If the connection fails, an error message will be printed to the debug
 output.
 *
 * @details
 * Upon successful connection, the function sets up callbacks for incoming
 publish messages and data
 * handling using `mqtt_set_inpub_callback`.

 */
err_t tls_mqtt_connect(MQTT_CLIENT_T *state);
/**
 * @brief Performs blocking DNS lookup. Works the same for a hostname and IP
 * string. Sets the client err_state accordingly
 *
 * @param state The MQTT client state
 * @param host The string with IP or a hostname
 */
void run_dns_lookup(MQTT_CLIENT_T *state, const char *host);

#endif // TLS_MQTT_CLIENT_SENTRY
