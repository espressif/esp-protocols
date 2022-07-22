VERSION=356fff8028734dc50a46972b2e315e2871b0a12e
mkdir -p ext_components
cd ext_components
wget https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/${VERSION}.zip
unzip ${VERSION}.zip
# rm -rf FreeRTOS-Kernel
mv -f FreeRTOS-Kernel-356fff8028734dc50a46972b2e315e2871b0a12e FreeRTOS-Kernel

rm ${VERSION}.zip
