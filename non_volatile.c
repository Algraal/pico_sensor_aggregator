#include "non_volatile.h"
#include "utility.h"

#include <boards/pico_w.h>
#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>
#include <hardware/regs/intctrl.h>
#include <hardware/sync.h>
#include <lwip/arch.h>
#include <pico.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <string.h>

static uint8_t number_of_segments(uint16_t length, int segment) {
  return (length + segment - 1) / segment;
}
void read_from_non_volatile(uint8_t *buffer, uint16_t length) {
  // points to the place on the flash with a data to be read. erasing flash
  // is performed in the flash memory space, but reading is possible
  // not only for the flash memory, that is why XIP_BASE offset is used to
  // reach the start of the flash + write_start
  uint8_t num_segments = number_of_segments(length, NON_VOL_SEGMENT_SIZE);
  int read_start =
      PICO_FLASH_SIZE_BYTES - (NON_VOL_SEGMENT_SIZE * num_segments) + XIP_BASE;
  char *flash_mem = (char *)read_start;
  memcpy(buffer, flash_mem, length);
  DEBUG_PRINT("\nread_from_non_volatile num_segments: %d, read_start: %d\n",
              num_segments, read_start);
}

int write_in_non_volatile(const uint8_t *data, uint16_t length) {
  uint8_t num_segments = number_of_segments(length, NON_VOL_SEGMENT_SIZE);
  // Seek the end, step offset
  int write_start =
      PICO_FLASH_SIZE_BYTES - (NON_VOL_SEGMENT_SIZE * num_segments);
  uint8_t number_of_pages = number_of_segments(length, NON_VOL_PAGE_SIZE);
  // START critical section, interrupts may interfere flash during writing (for
  // some reasion clangd writes that save_and_disable_interrupts is not defined
  // but it is not true)
  uint32_t status = save_and_disable_interrupts();
  multicore_lockout_start_blocking();
  // Erase memory on the end of the flash
  flash_range_erase(write_start, NON_VOL_SEGMENT_SIZE * num_segments);
  // Copy data
  flash_range_program(write_start, data, number_of_pages * NON_VOL_PAGE_SIZE);
  // Now leave the critical area
  multicore_lockout_end_blocking();
  restore_interrupts(status);
  DEBUG_PRINT(
      "num_segments: %d, write_start: %d, number_of_pages %d, read_start: %d\n",
      num_segments, write_start, number_of_pages, write_start + XIP_BASE);
}
