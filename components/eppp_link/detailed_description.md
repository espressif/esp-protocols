# ESP PPP Link Component (eppp_link) - Detailed Documentation

## Overview

The ESP PPP Link component provides a versatile communication bridge between two ESP32 microcontrollers, enabling network connectivity over various physical transport layers. One device acts as a server (typically providing connectivity), while the other acts as a client (consuming connectivity).

## Network Interface Modes

### PPP Mode vs TUN Mode

The component supports two distinct network interface modes:

#### PPP Mode (`CONFIG_EPPP_LINK_USES_PPP=y`)
- **Standard PPP Protocol**: Uses the Point-to-Point Protocol (RFC 1661) with full LCP negotiation
- **Compatibility**: Compatible with standard PPP implementations like Linux `pppd`
- **Features**:
  - Automatic IP address negotiation
  - Link Control Protocol (LCP) for connection establishment
  - Authentication support (if configured)
  - Standard PPP framing with escape sequences
- **Use Case**: When interfacing with standard PPP-capable systems or when full PPP compatibility is required
- **Transport Limitation**: Primarily designed for UART transport due to PPP's serial nature

#### TUN Mode (`CONFIG_EPPP_LINK_USES_PPP=n`, default)
- **Simplified Packet Interface**: Uses a custom packet-based protocol without PPP negotiation
- **Performance**: Faster data transfer due to reduced protocol overhead
- **Features**:
  - Direct IP packet transmission
  - Custom framing for packet boundaries
  - No negotiation overhead
  - Static IP address configuration
- **Use Case**: Default mode for ESP-to-ESP communication, optimal for non-UART transports
- **Transport Support**: Works efficiently with all transport types (UART, SPI, SDIO, Ethernet)

**Mode Selection**: Configure via `idf.py menuconfig` → `Component config` → `eppp_link` → `Use PPP network interface`

## Transport Layer Options

### UART Transport
- **Configuration**: `CONFIG_EPPP_LINK_DEVICE_UART=y`
- **Features**:
  - Simple serial communication
  - Configurable baud rate (up to 3Mbps tested)
  - Hardware flow control support
  - Custom framing for packet boundaries in TUN mode
- **Performance**: ~2 Mbps (TCP/UDP) @ 3 Mbaud
- **Use Case**: Basic connectivity, long-distance communication, debugging
- **Pins**: TX, RX configurable

```c
eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
config.transport = EPPP_TRANSPORT_UART;
config.uart.tx_io = 25;
config.uart.rx_io = 26;
config.uart.baud = 921600;
```

### SPI Transport
- **Configuration**: `CONFIG_EPPP_LINK_DEVICE_SPI=y`
- **Features**:
  - Master/slave configuration
  - GPIO interrupt signaling for flow control
  - Configurable clock frequency
  - Full-duplex communication
  - Packet queue for transmission
- **Performance**: ~5 Mbps (TCP), ~8 Mbps (UDP) @ 16MHz
- **Use Case**: High-speed local communication, PCB-level connections
- **Pins**: MOSI, MISO, SCLK, CS, interrupt GPIO

```c
eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
config.transport = EPPP_TRANSPORT_SPI;
config.spi.is_master = true;
config.spi.freq = 16000000;
config.spi.mosi = 11;
config.spi.miso = 13;
config.spi.sclk = 12;
config.spi.cs = 10;
config.spi.intr = 2;
```

### SDIO Transport
- **Configuration**: `CONFIG_EPPP_LINK_DEVICE_SDIO=y`
- **Features**:
  - Host/slave configuration
  - High-speed data transfer
  - 1-bit or 4-bit bus width
  - Hardware flow control
- **Performance**: ~9 Mbps (TCP), ~11 Mbps (UDP)
- **Use Case**: Highest throughput applications, module-to-module communication
- **Pins**: CLK, CMD, D0-D3 (configurable width)

```c
eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
config.transport = EPPP_TRANSPORT_SDIO;
config.sdio.is_host = true;
config.sdio.width = 4;
config.sdio.clk = 18;
config.sdio.cmd = 19;
config.sdio.d0 = 14;
// ... additional data pins
```

### Ethernet Transport
- **Configuration**: `CONFIG_EPPP_LINK_DEVICE_ETH=y`
- **Features**:
  - Direct MAC-to-MAC communication
  - Can work with or without PHY chips
  - Standard Ethernet framing
  - Automatic task management (no `eppp_perform()` needed)
- **Performance**: ~5 Mbps (TCP), ~8 Mbps (UDP) with internal EMAC
- **Use Case**: Board-to-board communication, integration with existing Ethernet infrastructure
- **Pins**: MDC, MDIO, plus PHY-specific pins

```c
eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
config.transport = EPPP_TRANSPORT_ETHERNET;
config.ethernet.mdc_io = 23;
config.ethernet.mdio_io = 18;
config.ethernet.phy_addr = 1;
config.ethernet.rst_io = 5;
```

## Channel Support (Multiple Logical Channels)

### Overview
Channel support allows multiplexing multiple independent data streams over a single transport connection. This enables applications to separate different types of data (e.g., network traffic, control commands, sensor data) into distinct logical channels.

### Configuration
- **Enable**: `CONFIG_EPPP_LINK_CHANNELS_SUPPORT=y`
- **Count**: `CONFIG_EPPP_LINK_NR_OF_CHANNELS` (1-8 channels, default 2)
- **Limitation**: Not available for Ethernet transport

### Channel Usage
```c
// Channel callback function type
typedef esp_err_t (*eppp_channel_fn_t)(esp_netif_t *netif, int nr, void *buffer, size_t len);

// Register channel callbacks
esp_err_t eppp_add_channels(esp_netif_t *netif,
                           eppp_channel_fn_t *tx,     // Transmit function pointer (output)
                           const eppp_channel_fn_t rx, // Receive callback (input)
                           void* context);             // User context

// Get user context
void* eppp_get_context(esp_netif_t *netif);
```

### Channel Example
```c
// Channel 0: Default network traffic (handled automatically)
// Channel 1: Control/chat messages
// Channel 2: WiFi data forwarding

static esp_err_t channel_receive(esp_netif_t *netif, int channel, void *buffer, size_t len)
{
    switch(channel) {
        case 1: // Control channel
            process_control_message(buffer, len);
            break;
        case 2: // WiFi channel
            forward_to_wifi(buffer, len);
            break;
        default:
            ESP_LOGE(TAG, "Unknown channel %d", channel);
            return ESP_FAIL;
    }
    return ESP_OK;
}

// Register channels
eppp_channel_fn_t tx_func;
eppp_add_channels(netif, &tx_func, channel_receive, user_context);

// Transmit on specific channel
tx_func(netif, 1, "Hello", 5);  // Send to channel 1
```

## API Reference

### Simple API (Recommended for most use cases)

#### Client Side
```c
esp_netif_t *eppp_connect(eppp_config_t *config);
```
- **Purpose**: Simplified client connection
- **Behavior**: Blocks until connection is established
- **Returns**: Configured network interface or NULL on failure
- **Use Case**: Simple applications that don't need fine-grained control

#### Server Side
```c
esp_netif_t *eppp_listen(eppp_config_t *config);
```
- **Purpose**: Simplified server listening
- **Behavior**: Blocks until client connects
- **Returns**: Configured network interface or NULL on failure
- **Use Case**: Simple applications that don't need fine-grained control

#### Connection Management
```c
void eppp_close(esp_netif_t *netif);
```
- **Purpose**: Close connection and cleanup resources
- **Behavior**: Stops tasks, closes transport, destroys network interface

### Advanced API (Manual Control)

#### Initialization
```c
esp_netif_t *eppp_init(eppp_type_t role, eppp_config_t *config);
```
- **Purpose**: Initialize endpoint without starting communication
- **Parameters**:
  - `role`: `EPPP_SERVER` or `EPPP_CLIENT`
  - `config`: Transport and network configuration
- **Returns**: Network interface handle
- **Use Case**: Applications needing manual control over connection lifecycle

#### Connection Control
```c
esp_err_t eppp_netif_start(esp_netif_t *netif);
esp_err_t eppp_netif_stop(esp_netif_t *netif, int stop_timeout_ms);
```
- **Purpose**: Manual network interface start/stop
- **Use Case**: Dynamic connection management, error recovery

#### Task Management
```c
esp_err_t eppp_perform(esp_netif_t *netif);
```
- **Purpose**: Single iteration of communication task
- **Returns**:
  - `ESP_OK`: Continue operation
  - `ESP_FAIL`: Operation failed but should continue
  - `ESP_ERR_TIMEOUT`: Stop operation requested
- **Use Case**: Task-less operation, integration with custom task schedulers
- **Note**: Not needed for Ethernet transport (has its own task)

#### Resource Management
```c
void eppp_deinit(esp_netif_t *netif);
```
- **Purpose**: Clean up resources without stopping tasks
- **Use Case**: Manual resource management

## Configuration Examples

### Basic Client-Server Setup

**Client (Host) Configuration:**
```c
void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
    config.transport = EPPP_TRANSPORT_UART;
    config.uart.tx_io = 25;
    config.uart.rx_io = 26;
    config.uart.baud = 921600;

    esp_netif_t *netif = eppp_connect(&config);
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to connect");
        return;
    }

    ESP_LOGI(TAG, "Connected successfully");
    // Use network interface for communication
}
```

**Server (Slave) Configuration:**
```c
void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize WiFi or other network interface
    init_wifi();

    eppp_config_t config = EPPP_DEFAULT_SERVER_CONFIG();
    config.transport = EPPP_TRANSPORT_UART;
    config.uart.tx_io = 26;  // Crossed with client
    config.uart.rx_io = 25;  // Crossed with client
    config.uart.baud = 921600;

    esp_netif_t *netif = eppp_listen(&config);
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to setup server");
        return;
    }

    // Enable NAT to share WiFi connection
    ESP_ERROR_CHECK(esp_netif_napt_enable(netif));
    ESP_LOGI(TAG, "Server ready");
}
```

### Advanced Manual Control
```c
void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
    config.task.run_task = false;  // Disable automatic task

    esp_netif_t *netif = eppp_init(EPPP_CLIENT, &config);
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to initialize");
        return;
    }

    // Start network interface
    ESP_ERROR_CHECK(eppp_netif_start(netif));

    // Custom task loop
    while (true) {
        esp_err_t ret = eppp_perform(netif);
        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGI(TAG, "Operation stopped");
            break;
        } else if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Operation failed: %s", esp_err_to_name(ret));
        }

        // Add custom processing here
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    eppp_deinit(netif);
}
```

### Multi-Channel Configuration
```c
typedef struct {
    eppp_channel_fn_t tx_func;
    esp_netif_t *netif;
} channel_context_t;

static esp_err_t channel_rx(esp_netif_t *netif, int channel, void *buffer, size_t len)
{
    ESP_LOGI(TAG, "Channel %d received %d bytes", channel, len);
    // Process channel data based on channel number
    return ESP_OK;
}

void setup_channels(void)
{
    eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
    esp_netif_t *netif = eppp_connect(&config);

    channel_context_t *ctx = calloc(1, sizeof(channel_context_t));
    ctx->netif = netif;

    // Register channel callbacks
    ESP_ERROR_CHECK(eppp_add_channels(netif, &ctx->tx_func, channel_rx, ctx));

    // Send data on channel 1
    const char *msg = "Hello on channel 1";
    ctx->tx_func(netif, 1, (void*)msg, strlen(msg));
}
```
