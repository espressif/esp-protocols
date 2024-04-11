#!/usr/bin/env bash

if [ -z "$1" ]; then
    echo "Usage $0 <SERVER_CN> [CLIENT_CN]"
    exit 1;
fi

SERVER_CN=$1
CLIENT_CN="${2-client_cn}"

echo "Server's CN: $SERVER_CN"
echo "Client's CN: $CLIENT_CN"

## First create our own CA
openssl genrsa  -out ca.key 2048
openssl req -new -x509 -subj "/C=CZ/CN=Espressif" -days 365 -key ca.key -out ca.crt

# Server side
openssl genrsa -out srv.key 2048
openssl req -out srv.csr -key srv.key -subj "/CN=$SERVER_CN" -new -sha256
openssl x509 -req -in srv.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out srv.crt -days 365 -sha256

# Client side
openssl genrsa  -out client.key 2048
openssl req -out client.csr -key client.key -subj "/CN=$CLIENT_CN" -new -sha256
openssl  x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365 -sha256

## Generate config options
# Client side:
CA_CRT=`cat ca.crt | sed '/---/d' | tr -d '\n'`
echo "CONFIG_ESP_WIFI_REMOTE_EPPP_SERVER_CA=\"$CA_CRT\""
CLIENT_CRT=`cat client.crt | sed '/---/d' | tr -d '\n'`
echo "CONFIG_ESP_WIFI_REMOTE_EPPP_CLIENT_CRT=\"$CLIENT_CRT\""
CLIENT_KEY=`cat client.key | sed '/---/d' | tr -d '\n'`
echo "CONFIG_ESP_WIFI_REMOTE_EPPP_CLIENT_KEY=\"$CLIENT_KEY\""
## Server side (here it uses the same CA)
echo "CONFIG_ESP_WIFI_REMOTE_EPPP_CLIENT_CA=\"$CA_CRT\""
SERVER_CRT=`cat srv.crt | sed '/---/d' | tr -d '\n'`
echo "CONFIG_ESP_WIFI_REMOTE_EPPP_SERVER_CRT=\"$SERVER_CRT\""
SERVER_KEY=`cat srv.key | sed '/---/d' | tr -d '\n'`
echo "CONFIG_ESP_WIFI_REMOTE_EPPP_SERVER_KEY=\"$SERVER_KEY\""
