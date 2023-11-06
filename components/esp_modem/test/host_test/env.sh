#!/bin/bash

idf_version=$1
component=$2

if [[ "$idf_version" == "release-v4.3" ]] && [[ "$component" == "esp_modem" ]]; then
  lwip=lwip-2.1.2
  lwip_uri=http://download.savannah.nongnu.org/releases/lwip
  lwip_contrib=contrib-2.1.0

  wget --no-verbose ${lwip_uri}/${lwip}.zip
  unzip -oq ${lwip}.zip
  wget --no-verbose ${lwip_uri}/${lwip_contrib}.zip
  unzip -oq ${lwip_contrib}.zip

  apt-get update && apt-get install -y gcc-8 g++-8
  update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 800 --slave /usr/bin/g++ g++ /usr/bin/g++-8
  rm /usr/bin/gcov && ln -s /usr/bin/gcov-8 /usr/bin/gcov
  export LWIP_PATH=`pwd`/$lwip
  export LWIP_CONTRIB_PATH=`pwd`/$lwip_contrib
fi
