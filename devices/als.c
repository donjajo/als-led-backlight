#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "../common.h"
#include "../devices.h"
#include "./als/scanelements.h"
#include "als.h"
#include "../config.h"

Device *alsdevice = NULL;

Device *alsgetdevice()
{
    return alsdevice;
}

/**
 * @brief Enable IIO scan_elements and buffering
 * 
 * @param path 
 * @return int 
 */
int alsenablebuffers(const char *path)
{
    char filepath[PATH_MAX] = {0};
    int bufferfd = -1, ret = 0;
    char data;
    struct scanelements *scanelements = alsgetscanelements();

    strcat(filepath, path);
    strcat(filepath, "/buffer/enable");

    bufferfd = open(filepath, O_RDWR);
    if (bufferfd == -1) {
        fprintf(stderr, "Error opening file for rw %s\n", filepath);
        perror("");
        goto result;
    }

    if (read(bufferfd, &data, sizeof(char)) <= 0) {
        fprintf(stderr, "Error reading file %s\n", filepath);
        perror("");
        goto result;
    }

    scanelements->bufferdefaultvalue = data;

    if (data == '1') {
        data = '0';
        // Disable buffer so we can enable scan elements. Else, write operations will fail on them
        if (write(bufferfd, &data, sizeof(char)) == -1) {
            fprintf(stderr, "Error writing file %s\n", filepath);
            perror("");
            goto result;
        }
    }

    if (!alsloadscanelements(path))
        goto result;

    // Enable back buffer
    data = '1';
    if (write(bufferfd, &data, sizeof(char)) == -1) {
        perror("Error writing buffer file");
        fprintf(stderr, "Unable to enable iio buffers for als\n");
    }

    ret = 1;
    scanelements->bufferfd = bufferfd;

    result:
        if (ret == 0) {
            if (bufferfd >= 0) {
                close(bufferfd);
            }
        }

        return ret;
}

void alsdestroydevice(void *)
{

    alsdestroyscanelements();
}

/**
 * @brief Load ambient light sensor device into a Device type
 * 
 * @param path 
 * @return Device* 
 */
Device *loadalsdevice(char *path)
{
    Device *result = NULL;
    Config config = getconfig();
    char devicename[51] = {0};
    char *namefile = strcat_(path, "/name");
    if (namefile == NULL)
        goto ret;

    FILE *f = fopen(namefile, "r");
    if (f == NULL) {
        // fprintf(stderr, "Error opening device file %s", namefile);
        goto ret;
    }

    fread(devicename, sizeof(char), 50, f);
    fclose(f);

    // Trim newlines that comes in filename
    rtrim(devicename, '\n');
    if (strcasecmp(devicename, "als") != 0)
        goto ret;

    result = mkdevice(AMBIENT_LIGHT_SENSOR, "als", 0, config.alslowthreshold, config.alshighthreshold, path, NULL, NULL, alsdestroydevice);

    ret:
        if (namefile != NULL) {
            free(namefile);
        }

        return result;
}

void *alswatchcallback(void *args)
{
    struct watcherthreadargs *_args = (struct watcherthreadargs *) args;
    Device *device = (Device *) _args->metadata;
    int fd = *((int *) _args->evt);
    struct dbuf *dbuf = _args->dbuf;
    size_t i;
    struct scanelement *element;
    struct scanelements *scanelements = alsgetscanelements();
    size_t sz = scanelements->totalbits / 8;
    unsigned char buffer[sz];
    ssize_t n = 0;
    Config config = getconfig();

    n = read(fd, &buffer, sizeof(unsigned char[sz]));

    if (n <= 0) {
        fprintf(stderr, "Reading of %s failed\n", device->path);
        perror("read() failed");
    }

    if (config.verbose >= ALS_VERBOSE_LEVEL_2)
        printf("Read %ld bytes from element\n", n);

    for (i = 0; i < scanelements->n; i++) {
        element = scanelements->elements[i];

        if (element->type == IN_ILLUMINANCE && element->enabled == '1') {
            int32_t *value = (int32_t *) buffer;
           
            // I need help here when other elements are enabled

            // *value = *value << element->index;
            *value = *value >> element->datatype.shifts;
            // *value = *value & ~(element->datatype.validbits/8);

            deviceupdate((size_t) *value, device, 0);
            adjustdevices(device, dbuf);

            if (config.verbose >= ALS_VERBOSE_LEVEL_1)
                printf("ALS Percentage: %lf\n", device->percentage);
        }
    }

    if (config.verbose >= ALS_VERBOSE_LEVEL_2)
        printf("Modified: %s, fd: %d\n", device->path, fd);

    if (config.verbose >= ALS_VERBOSE_LEVEL_2)
        printf("Exited thread: %ld\n", pthread_self());

    return NULL;
}

int scanals(struct dbuf *dbuf)
{
    int ret = 0;
    struct dirent *dirent;
    char devfile[PATH_MAX] = {0};
    Device *device = NULL;
    Config config = getconfig();
    char *sysfsdir = strcat_("/sys/bus/iio/devices", "/");
    if (sysfsdir == NULL) {
        goto close;
    }

    DIR *dir = opendir(sysfsdir);
    if (dir == NULL) {
        goto close;
    }

    while ((dirent = readdir(dir)) != NULL) {
        char *devicefile = strcat_(sysfsdir, dirent->d_name);

        if (devicefile == NULL) {
            fprintf(stderr, "Unable to determine als device");
            goto br;
        }

        device = loadalsdevice(devicefile);
        if (device != NULL) {
            if (config.verbose >= ALS_VERBOSE_LEVEL_1)
                printf("Detected Ambient Light Sensor Device\n");

            if (alsenablebuffers(devicefile) == 1) {
                strcat(devfile, "/dev/");
                strcat(devfile, dirent->d_name);

                // Replace new path to /dev/iio*
                memset(device->path, 0, sizeof(char[PATH_MAX]));
                strncpy(device->path, devfile, PATH_MAX);

                ret = adddevice(device, dbuf) != NULL;
                if (ret) {
                    // char b[32];
                    // size_t dd;
                    // int fd = open(devfile, O_RDONLY);
                    // if (fd == -1)
                    //     perror("");
                    // while ((dd = read(fd, b, 32)) > 0) {
                    //     printf("Read: %ld\n", dd);
                    // }
                
                    alsdevice = device;
                    watch(devfile, 0, alswatchcallback, device, NULL);
                }

                free(devicefile);
                // We only need one device at the moment
                break;
            } else {
                fprintf(stderr, "Unable to enable buffers for this device\n");
            }
        }

        br:
        free(devicefile);
    }

    close:
        if (dir != NULL)
            closedir(dir);

        if (sysfsdir != NULL) {
            free(sysfsdir);
        }

        if (ret == 0) {
            if (device != NULL)
                destorydevice(device);
        }

        return ret;
}