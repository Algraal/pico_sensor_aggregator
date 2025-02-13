#include "sensors.h"
#include "dht.h"
#include "ds18b20.h"
#include "hardware_config.h"
#include "utility.h"

#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Wrappers for init, axilliry, collect, functions to utilize the same
 * interface. To use the same interface real instances of the sensors (e.g.
 * objects created by libraries) should be placed into the custom data, and all
 * of the neccessary information (GPIO number, SPI interfaces etc) should be
 * defined here as static variable accessable in the wrapper to pass it to the
 * real functions or in hardware_config.h as macro definitions.
 */

/* DHT wrappers */
static uint8_t init_fn_dht(void **custom_data) {
  dht_t *dht_inst = (dht_t *)malloc(sizeof(dht_t));
  if (dht_inst == NULL) {
    return 1;
  }
  *custom_data = dht_inst;
  dht_init(dht_inst, DHT_MODEL, DHT_PIO, DHT_DATA_PIN, true);
  return 0;
}
/* Wait at least 25ms after calling this function before collecting data */
static void auxiliary_fn_dht(void **custom_data) {
  dht_start_measurement((dht_t *)(*custom_data));
}
static void collect_fn_dht(char *str, uint16_t size, void **custom_data) {
  float humidity;
  float temperature_c;
  dht_result_t result = dht_finish_measurement_blocking(
      (dht_t *)(*custom_data), &humidity, &temperature_c);
  if (result == DHT_RESULT_OK) {
    snprintf(str, size, "{\"r_humidity\":%.2f,\"r_temperature\":%.2f}",
             humidity, temperature_c);
  } else {
    snprintf(str, size, "{\"r_humidity\":\"null\",\"r_temperature\":\"null\"}");
  }
}
static void clean_fn_dht(void **custom_data) {
  if (custom_data == NULL || *custom_data == NULL) {
    return;
  }
  dht_t *dht = (dht_t *)(*custom_data);
  dht_deinit(dht);
  free(dht);
  *custom_data = NULL;
}
/*------------End of DHT--------------*/

/* DS18B20 wrappers */
static uint8_t init_fn_ds18b20(void **custom_data) {
  ds18b20_t *ds = (ds18b20_t *)malloc(sizeof(ds18b20_t));
  if (ds == NULL) {
    return 1;
  }
  *custom_data = ds;

  int res = ds18b20_init(ds, DS18B20_PIO, DS18B20_PIN);
  DEBUG_PRINT("ds18b20-init() return error: %d\n", res);
  return res;
}
/* Wait at least 1000ms after calling this function before collecting the data
 */
static void auxiliary_fn_ds18b20(void **custom_data) {
  ds18b20_convert((ds18b20_t *)*custom_data);
}
static void collect_fn_ds18b20(char *str, uint16_t size, void **custom_data) {
  ds18b20_t *ds = (ds18b20_t *)(*custom_data);
  uint8_t res = ds18b20_read_temperature(ds);
  if (res) {
    snprintf(str, size, "{\"w_temp\":\"null\"}");
  } else {
    snprintf(str, size, "{\"w_temp\":%.2f}", ds->temperature);
  }
}
static void clean_fn_ds18b20(void **custom_data) {
  if (custom_data == NULL || *custom_data == NULL) {
    return;
  }
  ds18b20_t *ds = (ds18b20_t *)(*custom_data);
  ds18b20_deinit(ds);
  free(ds);
  *custom_data = NULL;
}
/*------------End of DS18B20--------------*/

sensor_wrap_t sensors_arr[] = {
    {.topic_name = "dht11",
     .topic_data = "",
     .custom_data = NULL,
     .init_fn = init_fn_dht,
     .auxiliary_fn = auxiliary_fn_dht,
     .collect_fn = collect_fn_dht,
     .clean_fn = clean_fn_dht,
     .disconnected = true},
#if 0
                            {.topic_name = "ds18b20",
                             .topic_data = "",
                             .custom_data = NULL,
                             .init_fn = init_fn_ds18b20,
                             .auxiliary_fn = auxiliary_fn_ds18b20,
                             .collect_fn = collect_fn_ds18b20,
                             .clean_fn = clean_fn_ds18b20,
                             .disconnected = true}
#endif
};

void init_sensors() {
  for (uint i = 0; i < ARRAY_LENGTH(sensors_arr); i++) {
    sensor_wrap_t *sensor = &sensors_arr[i];
    sensor->disconnected = sensor->init_fn(&(sensor->custom_data));
    DEBUG_PRINT("Sensor number %d is %s\n", i,
                sensor->disconnected ? "disconnected" : "connected");
  }
}

void prepare_sensors() {
  for (uint i = 0; i < ARRAY_LENGTH(sensors_arr); i++) {
    sensor_wrap_t *sensor = &sensors_arr[i];
    if (!sensor->disconnected && sensor->auxiliary_fn != NULL) {
      sensor->auxiliary_fn(&(sensor->custom_data));
    }
  }
}

void collect_data_sensors() {
  for (uint i = 0; i < ARRAY_LENGTH(sensors_arr); i++) {
    sensor_wrap_t *sensor = &sensors_arr[i];
    if (!sensor->disconnected) {
      sensor->collect_fn(sensor->topic_data, sizeof(sensor->topic_data),
                         &(sensor->custom_data));
    }
  }
}

void transfer_data_sensors(transfer_sensor_data_function transfer_fn) {
  for (uint i = 0; i < ARRAY_LENGTH(sensors_arr); i++) {
    sensor_wrap_t *sensor = &sensors_arr[i];
    if (!sensor->disconnected) {
      // i corresponds to the topic number
      transfer_fn(sensor->topic_data, sizeof(sensor->topic_data), i);
    }
  }
}

void deinit_clean_sensors() {
  for (uint i = 0; i < ARRAY_LENGTH(sensors_arr); i++) {
    sensor_wrap_t *sensor = &sensors_arr[i];
    if (!sensor->disconnected) {
      sensor->clean_fn(&(sensor->custom_data));
    }
  }
}
