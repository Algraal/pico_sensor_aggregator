cmake_minimum_required(VERSION 3.13)

# !Build with cmake -DCMAKE_BUILD_TYPE=Debug .. For getting serial data about
# the process of the connection

# Add W type of the Pico in order to use WiFi capabilities Do not forget to
# include lwipopts.h It is represented in
# pico-examples/pico_w/wifi/lwipopts_common.h Copy and rename this file into
# project dir as lwipopts.h
set(PICO_BOARD pico_w)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# DEBUG capabilites
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
# Define DEBUG macro for DEBUG_PRINT
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(DEBUG)
endif()

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(my_mqtt C CXX ASM)
pico_sdk_init()

# Add libraries
add_subdirectory(ds18b20_pio)
add_subdirectory(dht_pio)

add_executable(
  ${CMAKE_PROJECT_NAME}
  main.c
  wifi_arch.c
  tls_mqtt_client.c
  runtime_settings.c
  non_volatile.c
  sensors.c
  access_point_httpd/dhcpserver/dhcpserver.c
  access_point_httpd/dnsserver/dnsserver.c
  access_point_httpd/http_control.c)

target_include_directories(
  ${CMAKE_PROJECT_NAME}
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}
          ${CMAKE_SOURCE_DIR}/certs
          ${CMAKE_CURRENT_SOURCE_DIR}/ds18b20_pio
          ${CMAKE_CURRENT_SOURCE_DIR}/access_point_httpd
          ${CMAKE_CURRENT_SOURCE_DIR}/access_point_httpd/dhcpserver
          ${CMAKE_CURRENT_SOURCE_DIR}/access_point_httpd/dnsserver)

target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  pico_cyw43_arch_lwip_poll
  pico_multicore
  pico_stdlib
  pico_mbedtls
  pico_lwip_mbedtls
  pico_lwip_mqtt
  pico_lwip_http
  ds18b20
  dht)

target_compile_definitions(
  ${CMAKE_PROJECT_NAME} PRIVATE NO_SYS=1 COUNTRY=CYW43_COUNTRY_FINLAND
                                AUTH=CYW43_AUTH_WPA2_MIXED_PSK)

# Enable USB communication
pico_enable_stdio_usb(${CMAKE_PROJECT_NAME} 1)
# Disable uart
pico_enable_stdio_uart(${CMAKE_PROJECT_NAME} 0)

pico_add_extra_outputs(${CMAKE_PROJECT_NAME})
