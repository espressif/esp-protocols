if [ -f ext_components/FreeRTOS-Kernel/croutine.c ]; then
export EXT_FREERTOS_DIR=`pwd`/ext_components/FreeRTOS-Kernel
else
echo "Not found! Please run ./install.sh"
fi;
