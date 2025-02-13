#include "wifi_arch.h"
#include "utility.h"
#include <boards/pico_w.h>
#include <cyw43_ll.h>
#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <pico/cyw43_arch.h>

int setup_ap(uint32_t country, const char *ssid, const char *pass,
             uint32_t auth) {
  int i = 0, res;
  res = cyw43_arch_init_with_country(country);
  if (res) {
    return res;
  }
  cyw43_arch_enable_ap_mode(ssid, pass, auth);

  return 0;
}
int setup_sta(uint32_t country, const char *ssid, const char *pass,
              uint32_t auth, const char *hostname, ip_addr_t *ip,
              ip_addr_t *mask, ip_addr_t *gw) {
  int i = 0, res;
  struct netif *net;
  if (cyw43_arch_init_with_country(country)) {
    return 1;
  }
  cyw43_arch_enable_sta_mode();
  // Set up the hostname and make sure the changes take place
  cyw43_arch_lwip_begin();
  net = &cyw43_state.netif[CYW43_ITF_STA];
  cyw43_arch_lwip_end();
  if (hostname != NULL) {
    cyw43_arch_lwip_begin();
    netif_set_hostname(net, hostname);
    netif_set_up(net);
    cyw43_arch_lwip_end();
  }
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
  while (i++ < CONNECT_ATTEMPTS) {
    res = cyw43_arch_wifi_connect_timeout_ms(ssid, pass, auth, 60000);
    if (res == 0) {
      break;
    }
    /* Sleep between connections */
    sleep_ms(5000);
  }
  if (res != 0) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    return 2;
  }
  int flashrate = 1000;
  int status = CYW43_LINK_UP + 1;
  while (status >= 0 && status != CYW43_LINK_UP) {
    int new_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (new_status != status) {
      status = new_status;
      flashrate = flashrate / (status + 1);
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    sleep_ms(flashrate);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    sleep_ms(flashrate);
  }
  if (status < 0) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
  } else {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    cyw43_arch_lwip_begin();
    if (ip != NULL) {
      netif_set_ipaddr(net, ip);
    }
    if (mask != NULL) {
      netif_set_netmask(net, mask);
    }
    if (gw != NULL) {
      netif_set_gw(net, gw);
    }
    cyw43_arch_lwip_end();
  }
  return status;
}
