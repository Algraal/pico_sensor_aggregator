# Overview

Pico W based sensor aggregator to get along with basics of IoT. I use this
project as a backlog of studying the embedded world.

Generally, there are some sensors (ds18b20, dht11, etc.) and an MQTT client to
send the data to the broker. Also, controllable devices (water and light)
are added to execute OFF/ON commands. It is also possible to switch the device
into AP mode to change Wi-Fi/MQTT settings. Settings are stored in the flash.

This project accumulates different technologies used for building IoT-capable
devices.

## Sources

![dht_pio Library](https://github.com/vmilea/pico_dht)
ds18b20 library is based on onewire from Pico SDK examples.
dhcp+dns servers for AP mode are taken from Pico SDK examples.
![mqtt-sni-patch Source](https://github.com/cniles/picow-iot)

## Network Stack

1. LWIP:
   - MQTT-client (tls_mqtt_client);
   - HTTPD (control_http);
   - NETIF;
   - LWIP poll;
   - Altcp_tls for TLS.

2. MBEDTLS 2-way-auth for MQTT-client (tls_mqtt_client).

3. STA mode is used in normal mode to pass the sensor data to the MQTT broker.

4. AP mode is used for changing settings of the Wi-Fi, MQTT client, and Pico
   Hostname. Moreover, HTTPD running in AP mode also shows sensor data.

5. To change mode, a reboot + holding the default_settings_button is required.

## Pico C SDK

1. **Multiprocessing:**
   - Both cores are utilized: core 0 for sensors, core 1 for the network.
   - A queue is used to pass sensor data from the sensor-core to the net-core.
   - Mutexes are used to store settings provided by the user.
   - Sensor core lockout is used to safely read the flash from the net core.

2. **Flash:**
   - Network settings are stored at the end of the flash (runtime_settings, non-volatile).
   - Network settings can be overwritten via a POST request.

3. **Watchdog** is used to reboot the Pico once the settings have been changed.

4. **Alarm Pool:**
   - Used on the network core to turn off water after 10 seconds.
   - Implemented with a mutex to prevent concurrency issues.

## Features

- **MQTT Client:**
  - Can run in TLS/NON-TLS mode (`#define ENABLE_TLS 1` in `crypto_consts.h`).
  - In TLS mode, certificates should be stored in `crypto_consts.h`.
  - `sed 's/$/\\n/' some.crt > nice_chars_crt.txt` can be used to format certificates.

- **Default Network Settings:**
  - Hardcoded in `crypto_consts.h` and used on the first boot.
  - Settings are stored in flash and only reset if corrupted (magic number mismatch).

- **MQTT Topics:**
  - Sensors publish data under `HOSTNAME/r_hum`, `HOSTNAME/r_temp`,
  `HOSTNAME/w_temp`, `HOSTNAME/moist`.
  - Control topics for toggling states: `HOSTNAME/control/light`, `HOSTNAME/control/water`.
  - Water control has an automatic timeout to prevent accidental flooding.

- **Web Server (HTTPD):**
  - Provides a control interface for toggling devices and updating network settings.
  - In AP mode, displays sensor data and allows configuration changes.

## Implementation Details

- **Sensor Data Handling:**
  - Sensor data is collected on core 0 and passed to core 1 via a queue.
  - Data is accessible to both MQTT and HTTPD for publishing and display.

- **Control Mechanism:**
  - Light and water control are implemented via MQTT subscriptions.
  - A mutex is used to synchronize access to the water control system.
  - Watering is secured with an alarm-based timeout mechanism.

- **Startup Sequence:**
  1. The system initializes hardware and sensors.
  2. If the default settings button is pressed, it enters AP mode.
  3. Otherwise, it starts in STA mode and connects to the MQTT broker.
  4. In STA mode, sensor data is published periodically.
  5. Incoming MQTT commands toggle device states.

## Wrong design patterns

Backlog of errors in this project:

- Dynamic allocation for storing MQTT messsages. Static Ring buffer for MQTT
messages waiting to be sent to the broker would be a better solution.
- Main module variables. A better idea to implement a struct to store necessary
variables, static instance of the struct and pass pointer to it different
translation units.
- Controllable devices should be moved to the sensor core for consistency,
another queue should be added to pass the server commands from the network-core.
