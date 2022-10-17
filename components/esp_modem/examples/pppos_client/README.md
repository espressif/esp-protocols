# PPPoS simple client example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

## Overview
This example shows how to act as a MQTT client after the PPPoS channel created by using [ESP-MQTT](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/protocols/mqtt.html) APIs.

## How to use this example

See the README.md file in the upper level `pppos` directory for more information about the PPPoS examples.

### USB DTE support

For USB enabled targets (ESP32-S2 and ESP32-S3), it is possible to connect to the modem device via USB.
1. In menuconfig, navigate to `Example Configuration->Type of serial connection to the modem` and choose `USB`.
2. Connect the modem USB signals to pin 19 (DATA-) and 20 (DATA+) on your ESP chip.

USB example uses Quactel BG96 modem device. BG96 needs a positive pulse on its PWK pin to boot-up.

This example supports USB modem hot-plugging and reconnection.

### Supported IDF versions

This example is only supported from `v4.1`, as this is the default dependency of `esp-modem` component.

USB example is supported from `v4.4`.
