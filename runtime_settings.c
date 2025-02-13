#include "runtime_settings.h"
#include "crypto_consts.h"
#include "non_volatile.h"
#include "utility.h"

#include <pico/stdlib.h>
#include <stdint.h>
#include <string.h>
/* Global array used for addressing tls_mqtt_settings fields by name */
static const field_info_t fields[] = {
    FIELD_ENTRY(tls_mqtt_settings, wifi_ssid),
    FIELD_ENTRY(tls_mqtt_settings, wifi_pass),
    FIELD_ENTRY(tls_mqtt_settings, tls_mqtt_broker_hostname),
    FIELD_ENTRY(tls_mqtt_settings, tls_mqtt_broker_port),
    FIELD_ENTRY(tls_mqtt_settings, tls_mqtt_broker_CN),
    FIELD_ENTRY(tls_mqtt_settings, tls_mqtt_client_id),
    FIELD_ENTRY(tls_mqtt_settings, tls_mqtt_client_name),
    FIELD_ENTRY(tls_mqtt_settings, tls_mqtt_client_password),
};

const field_info_t *get_settings_fields() { return fields; }

uint8_t get_settings_fields_count() {
  return sizeof(fields) / sizeof(fields[0]);
}

int read_settings_from_flash(tls_mqtt_settings *settings) {
  if (settings == NULL) {
    DEBUG_PRINT("read_settings_from_flash NULL pointer provided\n");
    return 1;
  }
  read_from_non_volatile((uint8_t *)settings, sizeof(tls_mqtt_settings));
  if (settings->flag != SETTINGS_FLAG) {
    DEBUG_PRINT("settings was not written to flash yet\n");
    return 2;
  }
  if (settings->flag != settings->end_flag) {
    DEBUG_PRINT("Data on flash is invalid\n");
    return 3;
  }
  DEBUG_PRINT("Enabled settings from the flash\n");
  return 0;
}

void write_settings_in_flash(tls_mqtt_settings *settings) {
  if (settings == NULL) {
    DEBUG_PRINT("Cannot write NULL to flash\n");
    return;
  }
  settings->flag = SETTINGS_FLAG;
  settings->end_flag = SETTINGS_FLAG;
  write_in_non_volatile((const uint8_t *)settings, sizeof(tls_mqtt_settings));
}

void initialize_default_settings(tls_mqtt_settings *settings) {
  assert(settings != NULL);
  static const tls_mqtt_settings temp_static = {
      .flag = 123456,
      .wifi_ssid = WIFI_SSID,
      .wifi_pass = WIFI_PASSWORD,
      .tls_mqtt_broker_hostname = MQTT_SERVER_HOST,
      .tls_mqtt_broker_port = BROKER_MQTT_PORT,
      .tls_mqtt_broker_CN = MQTT_SERVER_HOST,
      .tls_mqtt_client_id = PICO_HOSTNAME,
      .tls_mqtt_client_name = TLS_MQTT_CLIENT_NAME,
      .tls_mqtt_client_password = TLS_MQTT_CLIENT_PASS,
#if ENABLE_TLS
      .ca_cert = CA_CERT,
      .client_cert = CLIENT_CERT,
      .client_key = CLIENT_KEY,
#endif                   // !ENABLE_TLS
      .end_flag = 123456 // end flag should be equal flag that is equal
                         // SETTINGS_FLAG
  };
  memcpy(settings, &temp_static, sizeof(tls_mqtt_settings));
  DEBUG_PRINT("Enable default settings\n");
}
