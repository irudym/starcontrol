# starcontrol
Zephyr based application to control brightness of LED over BLE

# Installation
Get latest Zephyr SDK from https://www.zephyrproject.org/

After that build the application:
make pristine
make BOARD=arduino_101_factory

to flash arduino board use the following command
make BOARD=arduino_101_factory flash
