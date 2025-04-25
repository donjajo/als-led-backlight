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

## Config
Currently, it reads config file from `/etc/als-led-backlight.conf`. Copy the `config.sample` file in this repository to the location

## Running
```sh
# ./build/als-led-backlight
```
