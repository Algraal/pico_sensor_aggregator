#ifndef RUNTIME_SETTINGS_H_SENTRY
#define RUNTIME_SETTINGS_H_SENTRY

#include <lwip/arch.h>
#include <pico/stdlib.h>
#include <stdint.h>

// For sizes of the certs
#include "crypto_consts.h"
// Control if the flash memory was initialized already
#define SETTINGS_FLAG 0xA5A5A5

/**
 * @brief Structure to store metadata about configuration fields.
 *
 * This structure helps manage the properties of various fields within
 * a configuration structure, such as their name, offset within the structure,
 * and size in bytes.
 */
typedef struct {
  const char *field_name;
  uint16_t offset;
  uint16_t size;
} field_info_t;
/* Auxiliary macro for initializing a corresponding array of fields */
#define FIELD_ENTRY(struct_type, field)                                        \
  {#field, offsetof(struct_type, field), sizeof(((struct_type *)0)->field)}

/**
 * @brief Retrieves an array of field descriptors.
 *
 * This function returns a pointer to a constant array of field_info_t
 * structures, which contain metadata about various fields within a
 * configuration structure.
 *
 * @return Pointer to an array of field_info_t structures.
 */
const field_info_t *get_settings_fields();
/**
 * @brief Gets the number of field descriptors available.
 * @return Number of field descriptors as an unsigned 8-bit integer.
 */
uint8_t get_settings_fields_count();

/* Struct to store network settings */
typedef struct {
  int flag; // Set to SETTINGS_FLAG before saving into the flash
  char wifi_ssid[33];
  char wifi_pass[64];
  char tls_mqtt_broker_hostname[200];
  char tls_mqtt_broker_port[6];
  char tls_mqtt_broker_CN[200];
  char tls_mqtt_client_id[100];
  char tls_mqtt_client_name[100];
  char tls_mqtt_client_password[100];
#if ENABLE_TLS
  char ca_cert[CA_CERT_SIZE];
  char client_cert[CLIENT_CERT_SIZE];
  char client_key[CLIENT_KEY_SIZE];
#endif          // !ENABLE_TLS
  int end_flag; // end flag should be equal flag that is equal SETTINGS_FLAG
                // after the struct is recorded in the flash. It is a simple
                // check for data validness.
} tls_mqtt_settings;
/**
 * @brief Reads MQTT settings from non-volatile flash memory.
 *
 * This function reads `tls_mqtt_settings` from flash memory and validates its
 * integrity using a predefined flag. If the settings are invalid or have not
 * been written to flash memory, appropriate error codes are returned.
 *
 * @param[out] settings A pointer to a `tls_mqtt_settings` structure that will
 * be populated with the settings read from flash memory. The memory for this
 * structure must be allocated by the caller.
 *
 * @return
 *   - `0` on success.
 *   - `1` if a `NULL` pointer is provided.
 *   - `2` if the settings have not been written to flash memory yet.
 *   - `3` if the data on flash memory is invalid.
 *
 * @note
 * - The function uses a predefined flag (`SETTINGS_FLAG`) to verify the
 * validity of the settings.
 */
int read_settings_from_flash(tls_mqtt_settings *settings);
/**
 * @brief Writes MQTT settings to non-volatile flash memory.
 *
 * This function writes the provided `tls_mqtt_settings` structure to flash
 * memory. It ensures the validity of the data by setting the `flag` and
 * `end_flag` fields to a predefined value (`SETTINGS_FLAG`).
 *
 * @param[in] settings A pointer to a `tls_mqtt_settings` structure containing
 * the settings to be written to flash memory.
 */
void write_settings_in_flash(tls_mqtt_settings *settings);
/**
 * @brief Initializes MQTT settings with default values.
 *
 * This function populates the provided `tls_mqtt_settings` structure with
 * default values for Wi-Fi and MQTT configuration. It ensures that all fields
 * are set appropriately, including the `flag` and `end_flag` fields used for
 * validation.
 *
 * @param[out] settings A pointer to a `tls_mqtt_settings` structure to be
 * initialized. The memory for this structure must be allocated by the caller.
 *
 * @note
 * - The function uses compile-time constants to populate default values.
 * - The `assert` macro is used to ensure that a non-`NULL` pointer is
 * provided.
 */
void initialize_default_settings(tls_mqtt_settings *settings);

#endif // RUNTIME_SETTINGS_H_SENTRY
