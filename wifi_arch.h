#ifndef WIFI_ARCH_H_SENTRY
#define WIFI_ARCH_H_SENTRY

#include <cyw43_country.h>
#include <cyw43_ll.h>

#include <lwip/ip_addr.h>
#include <pico/stdlib.h>
#define CONNECT_ATTEMPTS 5

/**
 * @brief Initializes and sets up the Wi-Fi access point (AP) mode.
 *
 * This function initializes the CYW43 Wi-Fi module for the specified country
 * and enables access point (AP) mode with the provided SSID, password,
 * and authentication method.
 *
 * @param country The country code (e.g., CYW43_COUNTRY_USA) to set the Wi-Fi
 * region.
 * @param ssid The SSID (network name) for the access point.
 * @param pass The password for the access point. Can be NULL for open networks.
 * @param auth The authentication type (e.g., CYW43_AUTH_WPA2_MIXED_PSK).
 *
 * @return Returns 0 on success, or a non-zero error code if initialization
 * fails.
 *
 */
int setup_ap(uint32_t country, const char *ssid, const char *pass,
             uint32_t auth);
/**
 * @brief Initializes STA and connects to a provided Wi-Fi network.
 *
 * This function sets up the Wi-Fi interface with the specified parameters
 * and attempts to connect to the provided network. It handles configuration
 * of the device hostname and optional static IP settings (IP address,
 * subnet mask, and gateway). Connection attempts are retried a limited
 * number of times before failing.
 *
 * @param[in] country   The country code for Wi-Fi initialization (e.g.,
 * `CYW43_COUNTRY_US`).
 * @param[in] ssid      The SSID of the Wi-Fi network to connect to.
 * @param[in] pass      The password for the Wi-Fi network.
 * @param[in] auth      The authentication type (e.g.,
 * `CYW43_AUTH_WPA2_AES_PSK`).
 * @param[in] hostname  A pointer to the desired hostname for the device.
 * Pass `NULL` to skip setting the hostname.
 * @param[in] ip        A pointer to the static IP address to assign. Pass
 * `NULL` to use DHCP.
 * @param[in] mask      A pointer to the subnet mask to assign. Pass `NULL`
 * to use DHCP.
 * @param[in] gw        A pointer to the gateway address to assign. Pass
 * `NULL` to use DHCP.
 *
 * @return
 *   - `CYW43_LINK_UP` on successful connection and link establishment.
 *   - A negative error code if the connection fails after all attempts.
 *   - `1` if Wi-Fi initialization fails.
 *   - `2` if all connection attempts fail.
 *
 */
int setup_sta(uint32_t country, const char *ssid, const char *pass,
              uint32_t auth, const char *hostname, ip_addr_t *ip,
              ip_addr_t *mask, ip_addr_t *gw);

#endif
