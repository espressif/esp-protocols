# ESP32 Mosquitto Port

This is a lightweight port of the Mosquitto broker designed to run on the ESP32. It currently supports a single listener with TCP transport or TLS transport based on ESP-TLS library.

## Supported Options

The Espressif port supports a limited set of options (with plans to add more in future releases). These options can be configured through a structure passed to the `mosq_broker_run()` function. For detailed information on available configuration options, refer to the [API documentation](api.md).

## API

### Starting the Broker

To start the broker, call the `mosq_broker_run()` function with a properly configured settings structure. The broker operates in the context of the calling task and does not create a separate task.

It's recommended to analyze the stack size needed for the task, but in general, the broker requires at least 4 kB of stack size.

```c
mosq_broker_run(&config);
```

## Memory Footprint Considerations

The broker primarily uses the heap for internal data, with minimal use of static/BSS memory. It consumes approximately 60 kB of program memory.

- **Initial Memory Usage**: ~2 kB of heap on startup
- **Per Client Memory Usage**: ~4 kB of heap for each connected client

When using the broker with multiple clients, ensure sufficient heap space is available to handle scenarios where clients disconnect abruptly and reconnect. In cases of clean disconnections, the broker releases the memory immediately. However, when a client loses connection abruptly, the broker retains its connection information for some time before eventually freeing this memory. If the client reconnects during this period, an additional 4 kB of heap is allocated for the new connection while the memory for the previous connection is still being retained. Therefore, extra heap space is necessary to manage these abrupt reconnections smoothly.

## Testing

Extensive long-term tests have been conducted with the broker handling 5 clients simultaneously. Each client publishes messages every second to a different topic, while all clients subscribe to all topics. The test also included abrupt disconnections, where clients were suspended and reconnected after 10 seconds, to simulate real-world network instability.
