#ifndef SENSORS_SENTRY_H
#define SENSORS_SENTRY_H

#include <pico/stdlib.h>
#include <stdint.h>

/* Function types used by the sensor */
/* Returns 0 on success, other values on error */
typedef uint8_t (*init_function)(void **custom_data);
typedef void (*auxiliary_function)(void **custom_data);
typedef void (*collect_function)(char *str, uint16_t size, void **custom_data);
/* Performs deinitialization of the hardware initialized in corresponding
 * init_function. If necessary deallocates dynamic memory in the custom_data
 * and takes care of dangling pointer of the custom_data.
 */
typedef void (*clean_function)(void **custom_data);

/* Function should be defined in the main program to pass the sensor data
 * to a handler function, other core etc. Used for easier buffering before
 * logging or sharing of the net.
 */
typedef void (*transfer_sensor_data_function)(const char *str, uint size,
                                              uint8_t topic_number);
// Structure used to organize a set of sensors into the same interface
typedef struct {
  char topic_name[8];
  char topic_data[128];
  void *custom_data;
  init_function init_fn;
  auxiliary_function auxiliary_fn;
  collect_function collect_fn;
  clean_function clean_fn;
  bool disconnected;
} sensor_wrap_t;

/**
 * @brief Initializes all sensors by calling their respective initialization
 * functions.
 *
 * This function iterates over the array of sensors, calling their
 * initialization functions. The `disconnected` flag is set based on the result
 * of the initialization.
 */
void init_sensors();
/**
 * @brief Prepares sensors by calling auxiliary functions if necessary.
 *
 * This function iterates over the sensor array and invokes auxiliary functions
 * for sensors that are successfully connected and have an auxiliary function
 * defined.
 */
void prepare_sensors();
/**
 * @brief Collects data from all connected sensors.
 *
 * This function iterates through the sensor array, collecting data from each
 * sensor that is connected and storing it in the respective topic data field.
 */
void collect_data_sensors();
/**
 * @brief Transfers collected sensor data using a provided transfer function.
 *
 * @param transfer_fn Function pointer to handle the transfer of sensor data.
 *
 * This function iterates through the sensor array and calls the provided
 * transfer function for each connected sensor, passing its topic data, size
 * of the topic data, and topic number.
 */
void transfer_data_sensors(transfer_sensor_data_function transfer_fn);

/**
 * @brief Deinitializes and cleans up all connected sensors.
 *
 * This function iterates over the sensor array and invokes the cleanup function
 * for each connected sensor, releasing any allocated resources.
 */
void deinit_clean_sensors();

#endif // SENSORS_SENTRY_H
