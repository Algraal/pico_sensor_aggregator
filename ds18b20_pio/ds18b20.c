#include "ds18b20.h"
#include "onewire_library.h"
#include "ow_rom.h"

#include <hardware/pio.h>
#include <pico.h>
#include <pico/stdlib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int ds18b20_init(ds18b20_t *ds, PIO p, uint8_t pin) {
  uint offset;
  uint64_t romcode;
  uint8_t num_devs;

  assert(ds != NULL);
  memset(ds, 0, sizeof(ds18b20_t));

  OW *ow_inst = (OW *)malloc(sizeof(OW));
  if (ow_inst == NULL) {
    return 1;
  }
  // Check whether it is possible to add the pio program on provided
  // PIO instance
  if (!pio_can_add_program(p, &onewire_program)) {
    free(ow_inst);
    return 2;
  }
  offset = pio_add_program(p, &onewire_program);
  if (!ow_init(ow_inst, p, offset, pin)) {
    free(ow_inst);
    return 3;
  }
  ds->ow = ow_inst;
  // Only one sensor is handled
  num_devs = ow_romsearch(ow_inst, &romcode, 1, OW_SEARCH_ROM);
  if (!num_devs) {
    return 4;
  }
  return 0;
}

void ds18b20_deinit(ds18b20_t *ds) {
  OW *ow = ds->ow;
  if (ds == NULL) {
    return;
  }
  if (ow != NULL) {
    // Deinitialize OW instance
    pio_sm_set_enabled(ow->pio, ow->sm, false);
    pio_sm_set_consecutive_pindirs(ow->pio, ow->sm, ow->gpio, 1, false);
    pio_sm_unclaim(ow->pio, ow->sm);
    pio_remove_program(ow->pio, &onewire_program, ow->offset);
    free(ow);
  }
}
// Requires some time for the sensor after initializing convert signal
// to collect and process data. 1000ms is enough
void ds18b20_convert(ds18b20_t *ds) {
  int i;
  ow_reset(ds->ow);
  ow_send(ds->ow, OW_SKIP_ROM);
  ow_send(ds->ow, DS18B20_CONVERT_T);
}
static uint8_t CRC8_calculation(uint8_t *data, uint8_t len) {
  uint8_t i;
  uint8_t j;
  uint8_t temp;
  uint8_t databyte;
  uint8_t crc = 0;
  for (i = 0; i < len; i++) {
    databyte = data[i];
    for (j = 0; j < 8; j++) {
      temp = (crc ^ databyte) & 0x01;
      crc >>= 1;
      if (temp) {
        crc ^= 0x8C;
      }
      databyte >>= 1;
    }
  }
  return crc;
}

static int ds18b20_read_scratchpad(ds18b20_t *ds) {
  uint8_t i, crc;
  // To send a the new command:
  //  send a new init pulse (to send a new command)
  //  send SKIP_ROM command (MATCH_ROM for choosing a particular slave device)
  //  send READ_SCRATCHPAD command
  //  ow_reset (&ow);
  ow_reset(ds->ow);
  ow_send(ds->ow, OW_SKIP_ROM);
  ow_send(ds->ow, DS18B20_READ_SCRATCHPAD);

  // Read all of the bytes in the strachpad for CRC check. It is possible to
  // check only few bytes, stop, and then resume reading other bytes, device
  // will remember the last position until the next init pulse
  for (i = 0; i < DS18B20_DATA_LENGTH; i++) {
    ds->data[i] = ow_read(ds->ow);
  }
  // Check crc
  crc = CRC8_calculation(ds->data, DS18B20_DATA_LENGTH);
  return crc;
}

static float ds18b20_data_to_temperature(uint8_t low_byte, uint8_t high_byte) {
  int16_t temp_int;
  // Put two bytes together, that is why int16_t is used for temp
  temp_int = low_byte | (high_byte << 8);
  // Low order four bites are the fraction
  return (float)temp_int / 16;
}

uint8_t ds18b20_read_temperature(ds18b20_t *ds) {
  int res;
  // CRC should be 0
  res = ds18b20_read_scratchpad(ds);
  if (res) {
    return 1;
  }
  ds->temperature = ds18b20_data_to_temperature(ds->data[0], ds->data[1]);
  return 0;
}
