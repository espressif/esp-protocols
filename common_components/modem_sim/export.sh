#!/bin/bash

source $IDF_PATH/export.sh

export AT_CUSTOM_COMPONENTS="`pwd`/pppd_cmd"

cd modem_sim_esp32/esp-at

python -m pip install -r requirements.txt

python build.py reconfigure
