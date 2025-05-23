#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#include "../common.h"
#include "../devices.h"
#include "../watcher.h"
#include "../config.h"

char *getvendorname(char *dirname)
{
    if (dirname == NULL) {
        return dirname;
    }

    size_t i = 0;

    for (; dirname[i] != ':' && dirname[i] != '\0'; i++);

    char *buf = malloc(sizeof(char[i+1]));
    if (buf != NULL) {
        memset(buf, 0, sizeof(char[i+1]));
        memcpy(buf, &dirname[0], sizeof(char[i]));
    }

    return buf;
}

void adjust(float ambvalue, void *self)
{
    Device *device = (Device *) self;
    float prev;
    int writerfd = *((int *) device->meta);
    char data = '1';
    uint8_t nvalue = 0;
    Config config = getconfig();

    if (ambvalue > 100) {
        nvalue = device->min_value;
    } else {
        nvalue = roundf((100 - ambvalue) / (100 / (float) device->max_value));
    }

    if (nvalue != device->current_value) {
        data = nvalue + '0';

        if (write(writerfd, &data, sizeof(char)) <= 0) {
            fprintf(stderr, "Unable to adjust keyboard light\n");
            perror("");
        } else {
            prev = device->percentage;
            deviceupdate(nvalue, device, 0);

            if (config.verbose >= ALS_VERBOSE_LEVEL_1)
                printf("Keyboard light adjusted from: %f -> %f\n", prev, device->percentage);
        }
    }
}

void destroy(void *self)
{
    Device *device = (Device *) self;
    close(*(int *) device->meta);
    free(device->meta);
}

Device *loaddevice(char *path, char *vendor)
{
    char *brigfile = strcat_(path, "/brightness");
    uint8_t current = 0;
    char data;
    uint8_t max = 0;
    Device *result = NULL;
    int *writerfd = malloc(sizeof(int));

    if (writerfd == NULL) {
        perror("malloc() failed");

        goto ret;
    }

    if (brigfile == NULL) {
        goto ret;
    }

    *writerfd = open(brigfile, O_RDWR);
    if (*writerfd == -1) {
        fprintf(stderr, "Error opening file %s: ", brigfile);
        perror("");
        goto ret;
    }

    if (read(*writerfd, &data, sizeof(char)) <= 0) {
        perror("read() failed");
        close(*writerfd);

        goto ret;
    }

    current = (uint8_t) data - '0';
    free(brigfile);
    brigfile = NULL;

    char *maxfile = strcat_(path, "/max_brightness");
    FILE *f = fopen(maxfile, "r");
    if (f == NULL) {
        goto ret;
    }
    max = (uint8_t) fgetc(f) - '0';
    fclose(f);
    free(maxfile);

    result = mkdevice(KEYBOARD_BACKLIGHT, vendor, current, 0, max, path, adjust, writerfd, destroy);

    ret:
        if (brigfile != NULL)
            free(brigfile);

        return result;
}

void *watchcallback(void *a)
{
    struct watcherthreadargs *args = (struct watcherthreadargs *) a;
    Device *device = (Device *) args->metadata;
    struct inotify_event *evt = (struct inotify_event *) args->evt;
    char filepath[PATH_MAX+1] = {0};
    int fd;
    char data;
    ssize_t n;
    uint8_t ishdwrchng;
    uint16_t updateopt = 0;
    Config config = getconfig();

   
    ishdwrchng = strcasecmp(evt->name, "brightness_hw_changed") == 0;
    
    strncat(filepath, device->path, PATH_MAX);
    strcat(filepath, "/brightness");

    fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        perror("open() failed");
        fprintf(stderr, "Unable to open file for reading: %s\n", filepath);
        
        goto ret;
    }

    n = read(fd, &data, sizeof(char));
    if (n <= 0) {
        perror("read() failed");
        fprintf(stderr, "Unable to read file: %s\n", filepath);

        goto ret;
    }

    close(fd);

    data = data - '0';

    if (ishdwrchng) {
        if ((size_t) data > device->current_value)
            updateopt = DEV_UP_ALS_MIN;
        else
            updateopt = DEV_UP_ALS_MAX;
    }

    deviceupdate(data, device, updateopt);

    if (config.verbose >= ALS_VERBOSE_LEVEL_1)
        printf("Keyboard light adjusted to: %.0f\n", device->percentage);

    ret:
        if (config.verbose >= ALS_VERBOSE_LEVEL_2)
            printf("Exited thread~~: %ld\n", pthread_self());
        return NULL;
}

int scankbdbacklight(struct dbuf *dbuf)
{
    struct dirent *file;
    Config config = getconfig();

    // 17 is length of sysfs path
    char *sysfsdir = calloc(17+1, sizeof(char));

    if (sysfsdir == NULL) {
        perror("calloc() failed");

        return 0;
    }

    strcat(sysfsdir, "/sys/class/leds/");

    DIR *sysfs = opendir(sysfsdir);
    if (sysfs == NULL) {
        free(sysfsdir);
        perror("Unable to open sysfs directory");

        return 0;
    }

    size_t lfl = 0;
    while ((file = readdir(sysfs)) != NULL) {
        if (strstr(file->d_name, "kbd_backlight") != NULL) {
            char *vendor = getvendorname(file->d_name);
            Device *device;
            size_t l = strnlen(file->d_name, NAME_MAX);
            if (l > lfl) {
                sysfsdir = realloc(sysfsdir, sizeof(char[17+l]));
            }
            lfl = l;

            strcat(sysfsdir, file->d_name);
            device = loaddevice(sysfsdir, vendor);
            if (device != NULL) {
                if (config.verbose >= ALS_VERBOSE_LEVEL_1)
                    printf("Detected keyboard light with vendor: %s\n", device->vendor);

                if (adddevice(device, dbuf) != NULL) {
                    watch(sysfsdir, IN_MODIFY, watchcallback, device, NULL);
                }
            } else {
                fprintf(stderr, "Error loading device: %s", sysfsdir);
            }
            
            // Reset the full path for another device
            memset(&sysfsdir[16], 0, sizeof(char[l]));
            free(vendor);
        }
    }

    closedir(sysfs);
    free(sysfsdir);

    return 1;
}