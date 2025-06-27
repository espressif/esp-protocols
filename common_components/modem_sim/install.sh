#!/bin/bash

# Create directory "modem_sim_esp32", go inside it
# Usage: ./install.sh [platform] [module]

SCRIPT_DIR=$(pwd)
mkdir -p modem_sim_esp32
cd modem_sim_esp32

# Shallow clone https://github.com/espressif/esp-at.git
if [ ! -d "esp-at" ]; then
    git clone --depth 1 https://github.com/espressif/esp-at.git
else
    echo "esp-at directory already exists, skipping clone."
fi

cd esp-at

# Add esp-idf directory which is a symlink to the $IDF_PATH
if [ -z "$IDF_PATH" ]; then
    echo "Error: IDF_PATH environment variable is not set"
    exit 1
fi

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

# Override defaults if parameters are provided
if [ ! -z "$1" ]; then
    platform="$1"
fi
if [ ! -z "$2" ]; then
    module="$2"
fi

# Create file "build/module_info.json" with content
cat > build/module_info.json << EOF
{
    "platform": "$platform",
    "module": "$module",
    "description": "4MB, Wi-Fi + BLE, OTA, TX:17 RX:16",
    "silence": 0
}
EOF

cp "$SCRIPT_DIR/sdkconfig.defaults" "module_config/module_esp32_default/sdkconfig.defaults"

echo "Installation completed successfully!"
echo "Created modem_sim_esp32 directory with esp-at repository and configuration"
