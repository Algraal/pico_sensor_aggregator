/*
 * Header file enables operation on non-volatile memory to store mqtt and wifi
 * settings after restarts.
 * Pico W has 2 MB flash
 * The process of overwriting memory:
 *  1. Get the end of the flash memory (flash simple starts from 0x1000, but
 *  writing to the beginning will overwrite the program code, so first seek for
 *  the flash end). PICO_FLASH_SIZE_BYTES defines the size of the flash,
 * XIP_BASE defines the beginning of the Flash memory, so
 * XIP_BASE+PICO_FLASH_SIZE_BYTES will point to the end of Flash memory.
 *  2. Erase a memory segment (multiples 4096 bytes)
 *  3. Write page (multiples 256 bytes)
 */
#ifndef NON_VOLATILE_SENTRY
#define NON_VOLATILE_SENTRY

#include <pico/stdlib.h>

enum {
  NON_VOL_SEGMENT_SIZE = 4096,
  NON_VOL_PAGE_SIZE = 256,
};

/**
 * @brief Reads data from non-volatile memory into a buffer.
 *
 * This function reads a specified length of data from non-volatile memory,
 * starting at the calculated offset based on the flash memory size and the
 * required segments. The data is copied into the provided buffer.
 *
 * @param[out] buffer A pointer to the buffer where the data will be copied.
 * @param[in]  length The length of the data to be read in bytes.
 *
 * @note
 * - The function calculates the starting address for reading based on the
 * number of segments required for the data length and the flash memory layout.
 * - Debug messages include the number of segments and the read start address.
 */
void read_from_non_volatile(uint8_t *buffer, uint16_t length);
/**
 * @brief Writes data to non-volatile memory.
 *
 * This function writes a specified length of data to non-volatile memory. The
 * memory area is erased before writing to ensure a clean write operation.
 * Critical sections are protected to prevent interference during the write
 * process.
 *
 * @param[in] data   A pointer to the data to be written to non-volatile memory.
 * @param[in] length The length of the data to be written in bytes.
 *
 * @return
 * - `0` on success.
 * - A negative value if an error occurs (e.g., during flash programming).
 *
 * @note
 * - The function calculates the starting address for writing based on the
 * number of segments required for the data length and the flash memory layout.
 * - Flash memory is erased in segments and programmed in pages.
 * - The function disables interrupts and prevents multicore interference during
 * the critical section.
 * - Debug messages include details such as the number of segments, the starting
 * address, and the number of pages.
 */
int write_in_non_volatile(const uint8_t *data, uint16_t length);

#endif // NON_VOLATILE_SENTRY
