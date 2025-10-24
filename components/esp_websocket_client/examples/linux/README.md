# ESP Websocket Client - Host Example

This example demonstrates the ESP websocket client using the `linux` target. It allows for compilation and execution of the example directly within a Linux environment.

## Compilation and Execution

To compile and execute this example on Linux need to set target `linux`

* Debian/Ubuntu: `sudo apt-get install -y libbsd-dev`
* Fedora/RHEL: `sudo dnf install libbsd-devel`
* Arch: `sudo pacman -S libbsd`
* Alpine: `sudo apk add libbsd-dev`

```
idf.py --preview set-target linux
idf.py build
./build/websocket.elf
```

## Example Output

```
I (76826192) websocket: [APP] Startup..
I (76826193) websocket: [APP] Free memory: 4294967295 bytes
I (76826193) websocket: [APP] IDF version: v6.0-dev-2414-gab3feab1d13
I (76826195) websocket: Connecting to wss://echo.websocket.org...
W (76826195) websocket_client: `reconnect_timeout_ms` is not set, or it is less than or equal to zero, using default time out 10000 (milliseconds)
W (76826195) websocket_client: `network_timeout_ms` is not set, or it is less than or equal to zero, using default time out 10000 (milliseconds)
I (76826195) websocket: WEBSOCKET_EVENT_BEGIN
I (76826196) websocket_client: Started
I (76826294) esp-x509-crt-bundle: Certificate validated
I (76827230) websocket: WEBSOCKET_EVENT_CONNECTED
I (76827239) websocket: WEBSOCKET_EVENT_DATA
I (76827239) websocket: Received opcode=1
W (76827239) websocket: Received=Request served by 4d896d95b55478
W (76827239) websocket: Total payload length=32, data_len=32, current payload offset=0

I (76828198) websocket: Sending hello 0000
I (76828827) websocket: WEBSOCKET_EVENT_DATA
I (76828827) websocket: Received opcode=1
W (76828827) websocket: Received=hello 0000
W (76828827) websocket: Total payload length=10, data_len=10, current payload offset=0

I (76829207) websocket: Sending fragmented text message
```

## Coverage Reporting
For generating a coverage report, it's necessary to enable `CONFIG_GCOV_ENABLED=y` option. Set the following configuration in your project's SDK configuration file (`sdkconfig.ci.coverage`, `sdkconfig.ci.linux` or via `menuconfig`):
