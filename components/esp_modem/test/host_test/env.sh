#!/bin/bash

lwip=$1
lwip_uri=$2
lwip_contrib=$3

wget --no-verbose ${lwip_uri}/${lwip}.zip
unzip -oq ${lwip}.zip
wget --no-verbose ${lwip_uri}/${lwip_contrib}.zip
unzip -oq ${lwip_contrib}.zip
