#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "devices.h"
#include "devices/kbd.h"
#include "devices/als.h"
#include "config.h"

void devicecalculatepercentage(Device *dev)
{
    dev->percentage = (100/(float) dev->max_value) * (float) dev->current_value;
}

void deviceupdate(size_t current_value, Device *dev, uint8_t option)
{
    Device *alsdevice = alsgetdevice();
    Config config = getconfig();

    pthread_mutex_lock(&dev->mutex);

    dev->current_value = current_value;
    devicecalculatepercentage(dev);

    if (option && alsdevice != NULL && dev->type != AMBIENT_LIGHT_SENSOR) {
        if (config.verbose >= ALS_VERBOSE_LEVEL_2)
            printf("Current als_max = %d, als_min=%d\n", dev->als_max, dev->als_min);

        if (option == DEV_UP_ALS_MAX) {
            int32_t min = alsdevice->current_value - dev->als_max;

            if (min < 0)
                dev->als_min = 0;
            else
                dev->als_min += min;
            dev->als_max = alsdevice->current_value;
        }

        if (option == DEV_UP_ALS_MIN) {
            dev->als_max += alsdevice->current_value - dev->als_min;
            dev->als_min = alsdevice->current_value;
        }

        if (config.verbose >= ALS_VERBOSE_LEVEL_2)
            printf("New als_max = %d, als_min=%d, current=%ld\n", dev->als_max, dev->als_min, alsdevice->current_value);

    }

    pthread_mutex_unlock(&dev->mutex);
}

Device *mkdevice(enum Device_Type type, const char *vendor, size_t current_value, size_t min_value, size_t max_value, const char *path, void (*adjust)(float ambvalue, void *self), void *meta, void (*destorycallback)(void *self))
{
    Device *device = malloc(sizeof(Device));
    Config config = getconfig();

    if (device == NULL) {
        perror("malloc() failed");
        return device;
    }

    device->type = type;
    device->percentage = 0;
    device->min_value = min_value;
    device->max_value = max_value;
    memset(device->vendor, 0, sizeof(char[50]));
    strncpy(device->vendor, vendor, 50);
    device->adjust = adjust;
    device->meta = meta;
    device->destorycallback = destorycallback;
    memset(device->path, 0, sizeof(char[PATH_MAX]));
    strncpy(device->path, path, PATH_MAX);
    device->als_min = config.alslowthreshold;
    device->als_max = config.alshighthreshold;
    device->current_value = current_value;
    
    if (pthread_mutex_init(&device->mutex, NULL) != 0) {
        perror("pthread_mutex_init() failed");

        destorydevice(device);
        return NULL;
    }

    devicecalculatepercentage(device);
    
    return device;
}

void destorydevice(Device *device)
{
    if (device->destorycallback != NULL)
        device->destorycallback(device);

    pthread_mutex_destroy(&device->mutex);
    free(device);
}

struct dbuf *mkdbuf()
{
    struct dbuf *buf = malloc(sizeof(struct dbuf));
    if (buf == NULL) {
        perror("malloc() failed");
        return NULL;
    }

    buf->c = 0;
    buf->devices = NULL;

    return buf;
}

struct dbuf *adddevice(Device *device, struct dbuf *buf)
{
    buf->devices = realloc(buf->devices, sizeof(Device*[buf->c+1]));
    if (buf->devices == NULL) {
        perror("realloc() failed");
        return NULL;
    }

    buf->devices[buf->c++] = device;

    return buf;
}

void destorydbuf(struct dbuf *buf)
{
    while (buf->c--) {
        destorydevice(buf->devices[buf->c]);
    }

    if (buf->devices != NULL) {
        free(buf->devices);
    }

    free(buf);
}

void adjustdevices(Device * alsdevice, struct dbuf *dbuf)
{
    size_t i = 0;
    Device *device;
    float v;
    uint16_t currentvalue = alsdevice->current_value;

    for (i = 0; i < dbuf->c; i++) {
        device = dbuf->devices[i];
        
        if (device->adjust != NULL) {
            if (currentvalue > device->als_max)
                currentvalue = device->als_max;
            if (currentvalue < device->als_min)
                currentvalue = device->als_min;

            v = ((float) (currentvalue - device->als_min) / (device->als_max - device->als_min)) * 100.0f;
            device->adjust(v, device);
        }
    }
}

int scandevices(enum Device_Type type, struct dbuf *dbuf)
{
    switch (type) {
        case KEYBOARD_BACKLIGHT:
            return scankbdbacklight(dbuf);
            break;
        case AMBIENT_LIGHT_SENSOR:
            return scanals(dbuf);
        default:
            fprintf(stderr, "Unknown device type provided\n");
    }

    return 0;
}