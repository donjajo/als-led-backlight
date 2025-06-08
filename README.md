# als-led-backlight
Auto control of keyboard and LCD backlight (in-view) through Ambient Light Sensor in Linux

This program takes in data from the Ambient Light Sensor if found present on your laptop, and automatically adjust the light and its intensity on your keyboard if detected.

There is future plan to integrate LCD backlight too. 

## Requirements
- Linux
- gcc
- make

## Build
```sh
make
```

## Running
```sh
# ./build/als-led-backlight
```

## Installing
Install the this package as a service by running with root privilege:
```sh
make install
```

Check the status of the service with `systemctl status als-led-backlight`

## Config
Currently, it reads config file from `/etc/als-led-backlight.conf`. You can adjust the base reading threshold with this configuration

# Uninstalling
Uninstall with root privilege using:
```sh
make uninstall
```