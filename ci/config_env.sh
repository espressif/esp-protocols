#!/usr/bin/env bash
# This script is used to set some common variables for the CI pipeline.

set -e

# MQTT public broker URI
export CI_MQTT_BROKER_URI="broker.emqx.io"
