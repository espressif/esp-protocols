# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

def test_mosq_local(dut):
    """
    steps:
      1. Start mosquitto broker
      2. Start mqtt client
      3. Checks that the client connected to broker
      4. checks that the client received published data
    """
    # We expect the client to connect from IPv4 localhost address
    dut.expect('New client connected from 127.0.0.1')
    # We expect the client to get "connected" event and receive the published data
    dut.expect('MQTT_EVENT_CONNECTED')
    dut.expect('MQTT_EVENT_DATA')
    dut.expect('TOPIC=/topic/qos0')
    dut.expect('DATA=data')
