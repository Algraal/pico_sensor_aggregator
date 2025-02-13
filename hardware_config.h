#ifndef HARDWARE_CONFIG_H_SENTRY
#define HARDWARE_CONFIG_H_SENTRY

/*---SENSORS---*/

/* Water-proof DS18B20 temp sensor */
#define DS18B20_PIN 2
#define DS18B20_PIO pio1
/* DHT 11 with PIO-based library */
#define DHT_MODEL DHT11
#define DHT_DATA_PIN 0
#define DHT_PIO pio0

/*---CONTROL DEVICES---*/
#define CONTROL_BUFFER_SIZE 256
#define CONTROL_TOPIC_SIZE 9 ///< Maximum SSI tag size + NULL terminator
#define CONTROL_QUEUE_SIZE 4 ///< Maximum number of queued commands

/* 5V Water pump driven via the relay */
#define WATER_PIN 16
#define WATERING_TIME_MS 10000
/* Relay driven light */
#define LIGHT_PIN 7
#define DEFAULT_SETTINGS_BUTTON 10

#endif // !HARDWARE_CONFIG_H_SENTRY
