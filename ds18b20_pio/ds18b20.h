#ifndef DS18B20_SENTRY_H
#define DS18B20_SENTRY_H

#include "onewire_library.h"

#include <hardware/pio.h>
#include <pico/stdlib.h>
#include <stdint.h>

#define DS18B20_CONVERT_T 0x44
#define DS18B20_READ_SCRATCHPAD 0xBE
#define DS18B20_DATA_LENGTH 9

typedef struct {
  uint8_t data[DS18B20_DATA_LENGTH];
  float temperature;
  OW *ow;
} ds18b20_t;

/**
 * @brief Initializes the DS18B20 temperature sensor.
 *
 * This function initializes the DS18B20 sensor by setting up the PIO program
 * and searching for the connected device on the 1-Wire bus.
 *
 * @param ds Pointer to the ds18b20_t structure.
 * @param p The PIO instance to use.
 * @param pin The GPIO pin connected to the DS18B20 sensor.
 * @return 0 on success, non-zero error code on failure.
 */
int ds18b20_init(ds18b20_t *ds, PIO p, uint8_t pin);

/**
 * @brief Deinitializes the DS18B20 sensor and releases resources.
 *
 * This function deinitializes the DS18B20 sensor by disabling the PIO state
 * machine, releasing allocated resources, and resetting the GPIO pin to input
 * mode.
 *
 * @param ds Pointer to the ds18b20_t structure.
 */
void ds18b20_deinit(ds18b20_t *ds);

/**
 * @brief Sends a temperature conversion command to the DS18B20 sensor.
 *
 * This function initiates temperature measurement on the DS18B20 sensor.
 * The sensor requires approximately 1000 ms to complete the measurement.
 *
 * @param ds Pointer to the ds18b20_t structure.
 */
void ds18b20_convert(ds18b20_t *ds);

/**
 * @brief Reads the temperature from the DS18B20 sensor. Read the temperature
 * in the corresponding field of the ds18b20_t.
 *
 * This function reads the temperature data from the sensor's scratchpad memory
 * and converts it into a floating-point temperature value.
 *
 * @param ds Pointer to the ds18b20_t structure.
 * @return 0 on success, 1 if a CRC check failure occurs.
 */
uint8_t ds18b20_read_temperature(ds18b20_t *ds);

#endif // !DS18B20_SENTRY_H
