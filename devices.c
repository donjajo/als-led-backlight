#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "devices.h"
#include "devices/kbd.h"
#include "devices/als.h"

Device *mkdevice(enum Device_Type type, const char *vendor, float percentage, size_t min_value, size_t max_value, const char *path, void (*adjust)(float ambvalue, void *self), void *meta, void (*destorycallback)(void *self))
{
    Device *device = malloc(sizeof(Device));
    if (device == NULL) {
        perror("malloc() failed");
        return device;
    }

    device->type = type;
    device->percentage = percentage;
    device->min_value = min_value;
    device->max_value = max_value;
    memset(device->vendor, 0, sizeof(char[50]));
    strncpy(device->vendor, vendor, 50);
    device->adjust = adjust;
    device->meta = meta;
    device->destorycallback = destorycallback;
    memset(device->path, 0, sizeof(char[PATH_MAX]));
    strncpy(device->path, path, PATH_MAX);
    
    if (pthread_mutex_init(&device->mutex, NULL) != 0) {
        perror("pthread_mutex_init() failed");

        destorydevice(device);
        return NULL;
    }
    
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

void adjustdevices(float alspercent, struct dbuf *dbuf)
{
    size_t i = 0;
    Device *device;

    for (i = 0; i < dbuf->c; i++) {
        device = dbuf->devices[i];
        
        if (device->adjust != NULL) {
            device->adjust(alspercent, device);
        }
    }
}

int scandevices(enum Device_Type type, struct dbuf *dbuf, struct watcherbuf *watcherbuf)
{
    switch (type) {
        case KEYBOARD_BACKLIGHT:
            return scankbdbacklight(dbuf, watcherbuf);
            break;
        case AMBIENT_LIGHT_SENSOR:
            return scanals(dbuf, watcherbuf);
        default:
            fprintf(stderr, "Unknown device type provided\n");
    }

    return 0;
}