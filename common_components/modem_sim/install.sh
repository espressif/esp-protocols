#!/bin/bash
set -e

# Create directory "modem_sim_esp32", go inside it
# Usage: ./install.sh [platform] [module] [uart_tx_pin] [uart_rx_pin]

SCRIPT_DIR=$(pwd)
UPDATE_UART_PINS_SCRIPT="$(cd "$(dirname "$0")" && pwd)/update_uart_pins.py"
mkdir -p modem_sim_esp32
cd modem_sim_esp32

if [ -z "$IDF_PATH" ]; then
    echo "Error: IDF_PATH environment variable is not set"
    exit 1
fi

# Default ESP_AT_VERSION uses this specific commit from master to support new chips and features
ESP_AT_VERSION="aa9d7e0e9b741744f7bf5bec3bbf887cff033d5f"

# Shallow clone of esp-at.git at $ESP_AT_VERSION
if [ ! -d "esp-at" ]; then
    # cannot shallow clone from a specific commit, so we init, shallow fetch, and checkout
    mkdir -p esp-at && cd esp-at && git init && git remote add origin https://github.com/espressif/esp-at.git
    git fetch --depth 1 origin $ESP_AT_VERSION && git checkout $ESP_AT_VERSION
else
    echo "esp-at directory already exists, skipping clone."
    cd esp-at
fi

# Add esp-idf directory which is a symlink to the $IDF_PATH
if [ ! -L "esp-idf" ]; then
    ln -sf "$IDF_PATH" esp-idf
else
    echo "esp-idf symlink already exists, skipping."
fi

# Create "build" directory
mkdir -p build

# Default values for platform and module
platform="PLATFORM_ESP32"
module="WROOM-32"
uart_tx_pin=""
uart_rx_pin=""

# Override defaults if parameters are provided
if [ ! -z "$1" ]; then
    platform="$1"
fi
if [ ! -z "$2" ]; then
    module="$2"
fi
if [ ! -z "$3" ]; then
    uart_tx_pin="$3"
fi
if [ ! -z "$4" ]; then
    uart_rx_pin="$4"
fi

target="${platform##*_}"
target="${target,,}"

# Use provided pins for description when present; otherwise keep defaults
description="4MB, Wi-Fi + BLE, OTA, TX:17 RX:16"
if [ -n "$uart_tx_pin" ] || [ -n "$uart_rx_pin" ]; then
    desc_tx=${uart_tx_pin:-17}
    desc_rx=${uart_rx_pin:-16}
    description="4MB, Wi-Fi + BLE, OTA, TX:${desc_tx} RX:${desc_rx}"
fi

# Create file "build/module_info.json" with content
cat > build/module_info.json << EOF
{
    "platform": "$platform",
    "module": "$module",
    "description": "$description",
    "silence": 0
}
EOF

# Optionally update UART pins in factory_param_data.csv for the selected module
if [ -n "$uart_tx_pin" ] || [ -n "$uart_rx_pin" ]; then
    csv_path="components/customized_partitions/raw_data/factory_param/factory_param_data.csv"
    if [ ! -f "$csv_path" ]; then
        echo "Warning: $csv_path not found; skipping UART pin update."
    else
        python3 "$UPDATE_UART_PINS_SCRIPT" "$platform" "$module" "$uart_tx_pin" "$uart_rx_pin" "$csv_path"
        echo "Updated UART pins in $csv_path"
    fi
fi

# Copy the platform-specific sdkconfig.defaults file if it exists
if [ -f "$SCRIPT_DIR/${target}.sdkconfig.defaults" ]; then
    cp "$SCRIPT_DIR/${target}.sdkconfig.defaults" "module_config/module_${target}_default/sdkconfig.defaults"
fi

echo "Installation completed successfully!"
echo "Created modem_sim_esp32 directory with esp-at repository and configuration"
