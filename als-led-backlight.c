#include <stdio.h>

#include "common.h"
#include "devices.h"
#include "watcher.h"

int main()
{
    struct dbuf *devicesbuffer = mkdbuf();
    struct watcherbuf *watcherbuffer;
    int ret = 1;

    if (devicesbuffer == NULL) {
        fprintf(stderr, "Unable to create buffer space");

        goto close;
    }

    watcherbuffer = mkwatcherbuffer();
    if (watcherbuffer == NULL) {
        fprintf(stderr, "Error creating watcher buffer\n");

        goto close;
    }

    printf("Ambient Light Sensor LED Backlight app started\n");
    printf("Scanning available devices...\n");

    if (!scandevices(KEYBOARD_BACKLIGHT, devicesbuffer, watcherbuffer)) {
        fprintf(stderr, "Error detecting keyboard light\n");

        goto close;
    }

    if (!scandevices(AMBIENT_LIGHT_SENSOR, devicesbuffer, watcherbuffer)) {
        fprintf(stderr, "Error detecting ambient light sensor device\n");

        goto close;
    }

    ret = 0;


    initwatcher(watcherbuffer, devicesbuffer);
    destroywatcherbuffer(watcherbuffer);
    close:
        if (devicesbuffer != NULL)
            destorydbuf(devicesbuffer);

    return ret;
}