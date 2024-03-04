#!/usr/bin/env bash
set -e

check_files="
    include/esp_wifi_remote_api.h
    include/esp_wifi_remote_with_hosted.h
    esp_wifi_with_remote.c
    test/smoke_test/components/esp_hosted/esp_hosted_mock.c
    test/smoke_test/components/esp_hosted/include/esp_hosted_mock.h
    test/smoke_test/main/all_wifi_calls.c
    test/smoke_test/main/all_wifi_remote_calls.c
    include/esp_wifi_types_native.h
    Kconfig
    Kconfig.soc_wifi_caps.in
    "

for i in $check_files; do
    generated_header="${i##*/}"
    current_header="../$i"
    echo "Checking $current_header"
    diff -I 'FileCopyrightText' $generated_header $current_header
done;
