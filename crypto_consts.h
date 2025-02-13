#ifndef CRYPTO_CONSTS_H_
#define CRYPTO_CONSTS_H_

/* File that stores private data not supposed to be exposed */

// Turns on TLS related settings and configs
// #define ENABLE_TLS 1

#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "11111111"

#define AP_MODE_SSID "pico_sensors"
#define AP_MODE_PASS "password"

// Mutual auth setting
#define MQTT_SERVER_HOST "192.168.0.2"
#define PICO_HOSTNAME "PicoWClient1"
#define QOS 1
#define TLS_MQTT_CLIENT_NAME ""
#define TLS_MQTT_CLIENT_PASS ""

#ifndef ENABLE_TLS
#define BROKER_MQTT_PORT "1883"
#else
#define BROKER_MQTT_PORT "8883"

// Certificates related definitions

// For certs and key generated with 2048 option size below are sufficient
#define CA_CERT_SIZE 2048
#define CLIENT_CERT_SIZE 2048
#define CLIENT_KEY_SIZE 2048

#define CA_CERT                                                                \
  "-----BEGIN CERTIFICATE-----\n\
MIIDSzCCAjOgAwIBAgIUfmSOWxnq0jeEu3mBjJ49yibFFq0wDQYJKoZIhvcNAQEL\n\
...\
+uyAAFrUMOgK05UUw7e2CnKtTMdwY55U16ZrsYF41Q==\n\
-----END CERTIFICATE-----\n"

#define CLIENT_KEY                                                             \
  "-----BEGIN PRIVATE KEY-----\n\
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQC3xv7a+df1HOcH\n\
...\
i4rPCeImOAOeVc9K9U69zP+v\n\
-----END PRIVATE KEY-----\n"

#define CLIENT_CERT                                                            \
  "-----BEGIN CERTIFICATE-----\n\
MIIC4DCCAcgCFBpGdopyvkvVX6dJ19BJ7q0wYNmMMA0GCSqGSIb3DQEBCwUAMDUx\n\
...\
7/huABHz/P4ik59xFhVerPa29Ys=\n\
-----END CERTIFICATE-----\n"
#endif // !ENABLE_TLS

#endif // !CRYPTO_CONSTS_H_
