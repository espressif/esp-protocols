#!/usr/bin/env bash
set -e
echo "bin_dir: $1"

bin_dir="$1"
THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ESP_PEER_VERSION="9512eef258a45aafcaaa309b1a67505c8b500363"

ESP_PEER_URL="https://github.com/espressif/esp-webrtc-solution/archive/${ESP_PEER_VERSION}.zip"
ESP_PEER_DIR="${bin_dir}/esp-peer"
ZIP_PATH="${bin_dir}/esp-peer.zip"
EXTRACTED_DIR="${ESP_PEER_DIR}/esp-webrtc-solution-${ESP_PEER_VERSION}"
COMPONENTS_SRC="${EXTRACTED_DIR}/components"
COMPONENTS_DST="${ESP_PEER_DIR}/components"
PATCH_FILE_1="${THIS_DIR}/Add-default-event-to-http-client-handler.patch"
PATCH_FILE_2="${THIS_DIR}/Remove-deprecated-freeRTOS-header.patch"
PATCH_FILE_3="${THIS_DIR}/libpeer-Add-direct-dependency-to-libsrtp.patch"

# Download if not exists
if [ ! -d "$EXTRACTED_DIR" ]; then
    echo "Downloading esp-peer ${ESP_PEER_VERSION}..."
    wget -O "$ZIP_PATH" "$ESP_PEER_URL"
    unzip -o "$ZIP_PATH" -d "$ESP_PEER_DIR"
    patch -p1 -d "$EXTRACTED_DIR" < "$PATCH_FILE_1"
    patch -p1 -d "$EXTRACTED_DIR" < "$PATCH_FILE_2"
    patch -p1 -d "$EXTRACTED_DIR" < "$PATCH_FILE_3"
    mv ${EXTRACTED_DIR}/components ${ESP_PEER_DIR}
    mv ${ESP_PEER_DIR}/components/esp_webrtc/impl/peer_default ${ESP_PEER_DIR}/components
fi
