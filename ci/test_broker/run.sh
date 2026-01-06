#!/bin/bash

set -e

BROKER_PATH=components/mosquitto/examples/broker
SCRIPT_DIR=$(dirname $0)
cp ${SCRIPT_DIR}/sdkconfig.broker ${BROKER_PATH}/sdkconfig.broker
idf.py -B build -DSDKCONFIG=sdkconfig.broker -C ${BROKER_PATH} build

./build/broker.elf &
export MOSQ_PID=$!
echo "Broker started with PID ${MOSQ_PID}"
