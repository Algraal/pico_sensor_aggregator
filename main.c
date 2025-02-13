#include "dhcpserver.h"
#include "dnsserver.h"
#include "hardware_config.h"
#include "http_control.h"
#include "runtime_settings.h"
#include "sensors.h"
#include "tls_mqtt_client.h"
#include "utility.h"
#include "wifi_arch.h"
// Include secure information
#include "crypto_consts.h"

#include <cyw43.h>
#include <cyw43_configport.h>
#include <hardware/gpio.h>
#include <lwip/apps/httpd.h>
#include <lwip/arch.h>
#include <lwip/err.h>
#include <lwip/ip4_addr.h>
#include <lwip/ip_addr.h>
#include <mbedtls/ssl.h>
#include <pico.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>

#include <lwip/dns.h>
#include <lwip/pbuf.h>
#include <lwip/tcp.h>

#include <lwip/altcp_tcp.h>
#include <lwip/altcp_tls.h>
#include <lwip/apps/mqtt.h>
#include <lwip/apps/mqtt_priv.h>

#include <pico/time.h>
#include <pico/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
// Multicore capabilities
#include <pico/multicore.h>
#include <pico/mutex.h>
#include <pico/util/queue.h>
// Restart feature
#include <hardware/watchdog.h>
// Water portion to turn off water once user forgot to do it
#include <hardware/timer.h>

/* Restart the net core once settings are changed */
/* Rewritten in the HTTPD server callback */
static tls_mqtt_settings mqtt_settings;
volatile bool store_settings_flag = false;
volatile bool restore_settings_flag = false;
volatile bool settings_changed = false;

volatile bool reset_core = false;
mutex_t reset_core_mutex;

typedef struct {
  uint8_t topic_index;
  uint8_t data[128];
} queue_entry_t;

/* Queue used by sensor core to pass data to net core */
queue_t sensor_data_queue;

/* Sensor section initialization
 * Sensor topics are used to store data from the queue so HTTPD and MQTT client
 * can fetch the topics for SSI and Publish */
const char *sensor_topics[] = {
    "r_hum",
    "r_temp",
    "w_temp",
    "moist",
};
#define NUMBER_OF_SENSOR_TOPICS sizeof(sensor_topics) / sizeof(sensor_topics[0])
queue_entry_t current_sensor_data[NUMBER_OF_SENSOR_TOPICS];

/* Control section initialization
 * Control topics are topics that perform actions required by client */
const char control_topics[][8] = {"water", "light"};
#define NUMBER_OF_CONTROL_TOPICS                                               \
  sizeof(control_topics) / sizeof(control_topics[0])

/* Forward declaration */
typedef struct control_topic control_topic_t;
/* Function prototype used in control topic to perform some action on request */
typedef void (*control_command)(control_topic_t *topic);

struct control_topic {
  /* The same name is used in SSI tag and SSI tag is required to be <= 8 chars
   * is it possible to use MULTIPART option of the SSI to avoid this restriction
   * though */
  const char topic_name[8];
  char topic_data[10];
  control_command fn;
};

void set_water(control_topic_t *topic);
void set_light(control_topic_t *topic);

/* Global variables to secure watering */
/* Used to secure access from alarm/net callback */
static mutex_t water_pump_mutex;
/* Alarm ID used for water control */
static volatile alarm_id_t water_pump_alarm = 0;
/* Default pool is initialized on 0 core by default and it may be interrupted
 * by Pico SDK, so add extra alarm pool to be initialized on the net core */
static alarm_pool_t *alarm_net_pool = NULL;
/* Initialize the initial state of control topics */
control_topic_t current_control_state[NUMBER_OF_CONTROL_TOPICS] = {
    {"water", "OFF", set_water}, {"light", "OFF", set_light}};
/* Called on timeout for watering */
int64_t water_alarm_callback(alarm_id_t id, void *user_data);

void set_water(control_topic_t *topic) {
  bool open = mutex_try_enter(&water_pump_mutex, NULL);
  /* Is closed by alarm - should not happen, but alarm can be called during
   * set_water. In such a scenario, just return and let other set_water to deal
   * with water state. It is possible, because of toggle nature of set_water.
   * Generally, more complicated actions are required */
  if (!open) {
    return;
  }
  size_t size;
  if (water_pump_alarm) {
    alarm_pool_cancel_alarm(alarm_net_pool, water_pump_alarm);
    water_pump_alarm = 0;
  }
  // If water is OFF now turn it on
  if (!strcmp("OFF", topic->topic_data)) {
    // Secure watering using alarm
    water_pump_alarm = alarm_pool_add_alarm_in_ms(
        alarm_net_pool, WATERING_TIME_MS, water_alarm_callback, topic, true);
    if (water_pump_alarm >= 0) {
      gpio_put(WATER_PIN, true);
      size = sizeof("ON");
      // If form had OFF from SSI tag that means state should be changed to ON
      strncpy(topic->topic_data, "ON", size);
      topic->topic_data[size] = 0;
    } else {
      DEBUG_PRINT("Alarm cannot be set\n");
    }
  } else {
    gpio_put(WATER_PIN, false);
    size = sizeof("OFF");
    // Any other value will turn off
    strncpy(topic->topic_data, "OFF", size);
    topic->topic_data[size] = 0;
  }
  // Free the mutex
  mutex_exit(&water_pump_mutex);
}

int64_t water_alarm_callback(alarm_id_t id, void *user_data) {
  DEBUG_PRINT("Timer faired\n");
  control_topic_t *topic = (control_topic_t *)user_data;
  set_water(topic);
  // Zero value says to not reschedule the alarm
  return 0;
}

void set_light(control_topic_t *topic) {
  // If water is OFF now turn it on
  size_t size;
  if (!strcmp("OFF", topic->topic_data)) {
    gpio_put(LIGHT_PIN, true);
    size = sizeof("ON");
    // If form had OFF from SSI tag that means state should be changed to ON
    strncpy(topic->topic_data, "ON", size);
    topic->topic_data[size] = 0;
  } else {
    gpio_put(LIGHT_PIN, false);
    size = sizeof("OFF");
    // Any other value will turn off
    strncpy(topic->topic_data, "OFF", size);
    topic->topic_data[size] = 0;
  }
}

uint16_t sensor_ssi_handler(int i_index, char *pc_insert, int insert_len) {
  size_t printed = 0;
  /* SSI tags are merged sensor + control topics that are different objects.
   * First process sensor topics then control topics */
  if (i_index >= 0 && i_index < NUMBER_OF_SENSOR_TOPICS) {
    printed = snprintf(pc_insert, insert_len, "%s",
                       current_sensor_data[i_index].data);
  } else {
    DEBUG_PRINT("Unknown i_index\n");
  }
  return (uint16_t)printed;
}
/* This function is passed to httpd to parse incoming POST requests */
void process_post_field(const char *key, const char *value) {
  const field_info_t *fields = get_settings_fields();
  uint8_t num_fields = get_settings_fields_count();
  for (uint8_t i = 0; i < num_fields; i++) {
    if (strcmp(fields[i].field_name, key) == 0) {
      unsigned long value_size = strlen(value);
      if (value_size < fields[i].size && value_size > 0) {
        memcpy((uint8_t *)&mqtt_settings + fields[i].offset, value,
               value_size + 1);
        settings_changed = true;
      }
    }
  }
}
/* Preforms reading from the queue, copies acquired data to the
 * current_sensor_data to make them accessible for the network core */
bool try_read_data_from_queue() {
  // Queue is declared globally, no need to pass it
  // Create an instance of queue entry to stored popped value
  queue_entry_t temp;
  bool ret = queue_try_remove(&sensor_data_queue, &temp);
  if (ret) {
    memcpy(&current_sensor_data[temp.topic_index], &temp, sizeof(temp));
    DEBUG_PRINT("ID: %d, DATA: %s\n", temp.topic_index, temp.data);
  }
  return ret;
}
/* Custom function passed to the MQTT client to perform server command */
void server_command_handler(uint8_t topic_number, const uint8_t *data,
                            size_t len) {
  current_control_state[topic_number].fn(&current_control_state[topic_number]);
}

err_t publish_topic_data(MQTT_CLIENT_T *state) {
  char full_topic[108];
  err_t err;
  /* Publish all sensor data */
  for (int i = 0; i < NUMBER_OF_SENSOR_TOPICS; i++) {
    queue_entry_t *current_sensor_record = &current_sensor_data[i];
    sprintf(full_topic, "%s/%s", state->settings->tls_mqtt_client_id,
            sensor_topics[i]);
    err = tls_mqtt_publish(state, full_topic, current_sensor_record->data,
                           strlen((char *)current_sensor_record->data), QOS);
    if (err != ERR_OK) {
      DEBUG_PRINT("publish topic data error: %d\n", err);
      return err;
    }
  }
  for (int i = 0; i < NUMBER_OF_CONTROL_TOPICS; i++) {
    sprintf(full_topic, "%s/%s", state->settings->tls_mqtt_client_id,
            current_control_state[i].topic_name);

    err = tls_mqtt_publish(
        state, full_topic, (uint8_t *)current_control_state[i].topic_data,
        strlen((char *)current_control_state[i].topic_data), QOS);
    if (err != ERR_OK) {
      DEBUG_PRINT("publish topic data error: %d\n", err);
      return err;
    }
  }
  return ERR_OK;
}

// Initialization from net core to serve incoming commands
void init_net_hardware() {
  /* Default settings button */
  gpio_init(DEFAULT_SETTINGS_BUTTON);
  gpio_set_dir(DEFAULT_SETTINGS_BUTTON, GPIO_IN);
  gpio_pull_up(DEFAULT_SETTINGS_BUTTON);
  /* Water */
  /* Initiazlie mutex for secure access */
  mutex_init(&water_pump_mutex);
  alarm_net_pool = alarm_pool_create_with_unused_hardware_alarm(1);
  DEBUG_PRINT("Alarm core: %d\n", alarm_pool_core_num(alarm_net_pool));
  gpio_init(WATER_PIN);
  gpio_set_dir(WATER_PIN, GPIO_OUT);
  gpio_put(WATER_PIN, 0);
  /* Light */
  gpio_init(LIGHT_PIN);
  gpio_set_dir(LIGHT_PIN, GPIO_OUT);
  gpio_put(LIGHT_PIN, 0);
}

static void httpd_ap_mode() {
  ip_addr_t gw;
  ip4_addr_t mask;
  dhcp_server_t dhcp_server;
  dns_server_t dns_server;
  /* Copy settings are used to store settings entered with POST request */
  tls_mqtt_settings *copy_settings =
      (tls_mqtt_settings *)malloc(sizeof(tls_mqtt_settings));
  if (copy_settings == NULL) {
    DEBUG_PRINT("Error allocating memory for settings\n");
    return;
  }
  // Deep copy is performed as settings are arrays of chars
  memcpy(copy_settings, &mqtt_settings, sizeof(tls_mqtt_settings));

  int res = setup_ap(COUNTRY, AP_MODE_SSID, AP_MODE_PASS, AUTH);
  if (res) {
    DEBUG_PRINT("Error setting up AP mode\n");
  } else {
    IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    // Start the dhcp server
    dhcp_server_init(&dhcp_server, &gw, &mask);
    // Start the dns server
    dns_server_init(&dns_server, &gw);
    my_httpd_run(sensor_ssi_handler, sensor_topics, NUMBER_OF_SENSOR_TOPICS,
                 process_post_field, &store_settings_flag);
  }
  while (true) {
    while (try_read_data_from_queue()) {
    }
    if (store_settings_flag) {
      if (settings_changed) {
        break;
      } else {
        store_settings_flag = false;
      }
    }
#if PICO_CYW43_ARCH_POLL
    cyw43_arch_poll();
    cyw43_arch_wait_for_work_until(make_timeout_time_ms(200));
#else
    sleep_ms(200);
#endif
  }
  free(copy_settings);
  dns_server_deinit(&dns_server);
  dhcp_server_deinit(&dhcp_server);
  cyw43_arch_deinit();
  if (store_settings_flag) {
    store_settings_flag = false;
    write_settings_in_flash(&mqtt_settings);
    // Blocking entrance to ensure other core will not skip check
    mutex_enter_blocking(&reset_core_mutex);
    reset_core = true;
    mutex_exit(&reset_core_mutex);
  }
}

void mqtt_sta_mode() {
  absolute_time_t timeout = nil_time;
  int res;
  TLS_MQTT_RET ret;
  MQTT_CLIENT_T *state;
  res = setup_sta(COUNTRY, mqtt_settings.wifi_ssid, mqtt_settings.wifi_pass,
                  AUTH, mqtt_settings.tls_mqtt_client_id, NULL, NULL, NULL);
  state = NULL;
  ret = tls_mqtt_init(&state, &mqtt_settings, server_command_handler);
  if (ret == TLS_MQTT_OK) {
    // After connection mqtt client will perform other actions via callbacks
    ret = tls_mqtt_connect(state);
  }
  while (true) {
    absolute_time_t now = get_absolute_time();
    while (try_read_data_from_queue()) {
    }
    if (is_nil_time(timeout) || absolute_time_diff_us(now, timeout) <= 0) {
      if (state->is_connected) {
        publish_topic_data(state);
        timeout = make_timeout_time_ms(3000);
      }
    }
#if PICO_CYW43_ARCH_POLL
    cyw43_arch_poll();
    cyw43_arch_wait_for_work_until(make_timeout_time_ms(200));
#else
    sleep_ms(200);
#endif
  }
}
/* Entry into the net responsible core
 * Reads settings, starts HTTP and MQTT, performs servicing HTTPD and MQTT
 * client*/
void core1_entry() {
  int res;
  TLS_MQTT_RET ret;
  MQTT_CLIENT_T *state;
  // Initialize hardware running on net core
  init_net_hardware();
  sleep_ms(50);
  // Button is low when it is pressed
  bool button_pressed = gpio_get(DEFAULT_SETTINGS_BUTTON) == 0;
  /* Perform reading settings from the flash or initialize default */
  res = read_settings_from_flash(&mqtt_settings);
  if (res) {
    /* Settings in the flash are corrupted or it is the first boot of the
     * device. Set the default settings hardcoded in definitions */
    initialize_default_settings(&mqtt_settings);
    write_settings_in_flash(&mqtt_settings);
  }
  /* Turn into httpd in AP mode */
  if (button_pressed) {
    httpd_ap_mode();
  } else {
    mqtt_sta_mode();
  }
}

static void pass_sensor_data_to_queue(const char *str, uint size,
                                      uint8_t topic_number) {
  queue_entry_t q_entry = {.topic_index = topic_number};
  assert(sizeof(q_entry.data) >= size);
  strncpy((char *)q_entry.data, str, sizeof(q_entry.data));
  DEBUG_PRINT("\n%s\n", q_entry.data);
  queue_add_blocking(&sensor_data_queue, &q_entry);
}

int main() {
  stdio_init_all();
  sleep_ms(4000);
  // Init queue to send data from sensors collected on the 0 core to the 1
  // core
  queue_init(&sensor_data_queue, sizeof(queue_entry_t), 10);
  // Initialize mutex responsible for restarting net core
  mutex_init(&reset_core_mutex);
  // Lock 0 core if the 1 core is going to write into the flash
  multicore_lockout_victim_init();
  multicore_launch_core1(core1_entry);
  bool restart = false;
  init_sensors();
  sleep_ms(2000);
  while (true) {
    DEBUG_PRINT("New main iteration\n");
    prepare_sensors();
    DEBUG_PRINT("prepare_sensors()\n");

    sleep_ms(2000);
    collect_data_sensors();
    DEBUG_PRINT("collect_data_sensors\n");

    transfer_data_sensors(pass_sensor_data_to_queue);
    DEBUG_PRINT("transfer_data_sensors()\n");

    /* Check whether the Pico core needs to be restarted
     * restart is needed when settings are stored and Pico needs to reconnect
     */
    mutex_enter_blocking(&reset_core_mutex);
    restart = reset_core;
    if (reset_core) {
      reset_core = false;
      deinit_clean_sensors();
    }
    mutex_exit(&reset_core_mutex);
    if (restart) {
      DEBUG_PRINT("Waiting to be rebooted by the watchdog\n");
      watchdog_enable(100, 0);
      // Just wait the restart
      while (1) {
      }
    }
    sleep_ms(3000);
  }
  return 0;
}
