#include <stdint.h>
#include <pthread.h>
#include <limits.h>

#include "watcher.h"

#ifndef _DEVICES_H
    #define _DEVICES_H

    #define DEV_UP_ALS_MAX 1
    #define DEV_UP_ALS_MIN 2

    enum Device_Type {
        KEYBOARD_BACKLIGHT,
        LCD_BACKLIGHT,
        AMBIENT_LIGHT_SENSOR
    };

    typedef struct {
        enum Device_Type type;
        char vendor[50];
        float percentage;
        size_t current_value;
        size_t max_value;
        size_t min_value;
        uint32_t als_max;
        uint32_t als_min;
        char path[PATH_MAX];
        void (*adjust)(float ambvalue, void *self);
        void *meta;
        void (*destorycallback)(void *self);
        pthread_mutex_t mutex;
    } Device;

    struct dbuf {
        Device **devices;
        size_t c;
    };

    int scandevices(enum Device_Type type, struct dbuf *dbuf);
    Device *mkdevice(enum Device_Type type, const char *vendor, size_t current_value, size_t min_value, size_t max_value, const char *path, void (*adjust)(float ambvalue, void *self), void *meta, void (*destorycallback)(void *self));
    void destorydevice(Device *device);
    struct dbuf *mkdbuf();
    void destorydbuf(struct dbuf *buf);
    struct dbuf *adddevice(Device *device, struct dbuf *buf);
    void adjustdevices(Device * alsdevice, struct dbuf *dbuf);
    void deviceupdate(size_t current_value, Device *dev, uint8_t option);
#endif