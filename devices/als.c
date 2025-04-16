#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include "../common.h"
#include "../devices.h"
#include "als.h"

struct scanelements scanelements = {NULL, 0, '0', -1};

int alsaddscanelement(struct scanelement *element)
{
    scanelements.elements = realloc(scanelements.elements, sizeof(struct scanelement*[scanelements.n + 1]));
    if (scanelements.elements == NULL) {
        perror("realloc() failed");
        return 0;
    }

    scanelements.elements[scanelements.n++] = element;

    return 1;
}

void alsdestroyscanelement(struct scanelement *element)
{
    if (element->enabled != '0' && element->defaultvalue != element->enabled)
        write(element->fd, &element->defaultvalue, sizeof(char));

    close(element->fd);

    free(element);
}

void alsdestroyscanelements()
{
    char data = '0';

    if (scanelements.bufferfd >= 0)
        write(scanelements.bufferfd, &data, sizeof(char));

    while (scanelements.n--) {
        alsdestroyscanelement(scanelements.elements[scanelements.n]);
    }

    if (scanelements.bufferfd >= 0) {
        if (scanelements.bufferdefaultvalue != '1')
            write(scanelements.bufferfd, &scanelements.bufferdefaultvalue, sizeof(char));

        close(scanelements.bufferfd);
    }

    if (scanelements.elements != NULL)
        free(scanelements.elements);
}

struct scanelement *alsmkscanelement(const char *name, char enabled, char defaultvalue, int fd)
{
    struct scanelement *element = malloc(sizeof(struct scanelement));
    if (element == NULL) {
        return element;
    }

    element->enabled = enabled;
    element->defaultvalue = defaultvalue;
    element->fd = fd;
    element->datatype = (struct elementdatatype) {0, 0, 0, 0, 0, 0};
    memset(element->name, 0, sizeof(char[NAME_MAX]));
    strncpy(element->name, name, NAME_MAX);

    return element;
}

/**
 * @brief Parser for scan_elements/_type format
 * 
 * @param element 
 * @param buffer 
 * @return int 
 */
int parsedatatypebuffer(struct scanelement *element, char *buffer)
{
    size_t i, j;
    char buf[ALS_DATATYPE_BUFSIZE] = {0};
    uint8_t hasrepeat = 0;
    int ret = 0;

    // char buffer[] = "le:s12/16X3>>4";
    rtrim(buffer, '\n');

    for (i = 0; buffer[i] != '\0'; i++) {
        if (i == 1) {
            element->datatype.endianness = buffer[0] == 'b' ? BIGENDIAN : LITTLEENDIAN;
            continue;
        }

        if (i == 3) {
            element->datatype.twoscompl = buffer[i] == 'u' ? UNSIGNEDINT : SIGNEDINT;
            continue;
        }

        if (i > 3) {
            if (buffer[i] == '/') {
                memcpy(buf, &buffer[4], sizeof(char[i-4]));
                j = i+1;
                element->datatype.validbits = atoi(buf);
                
                goto done;
            }

            if (buffer[i] == 'X') {
                memcpy(buf, &buffer[j], sizeof(char[i - j]));
                element->datatype.bitsize = atoi(buf);
                hasrepeat = 1;
                j = i+1;
                goto done;
            }

            if (buffer[i] == '>') {
                memcpy(buf, &buffer[j], sizeof(char[i - j]));

                if (hasrepeat == 0) {
                    element->datatype.bitsize = atoi(buf);
                } else {
                    element->datatype.repeat = atoi(buf);
                }

                j = i = i+2;
                ret = 1;
                goto done;
            }
        }

        done:
        memset(buf, 0, sizeof(char[ALS_DATATYPE_BUFSIZE]));
    }

    if (ret == 1) {
        memcpy(buf, &buffer[j], sizeof(char[i - j]));
        element->datatype.shifts = atoi(buf);
    }

    // printf("Endian: %d\nSigned: %d\nvalidbits: %u\ntotalbits: %u\nRepeat:%u\nshift: %u\n", element->datatype.endianness, element->datatype.twoscompl, element->datatype.validbits, element->datatype.bitsize, element->datatype.repeat, element->datatype.shifts);
    return ret;
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
    size_t i, len;
    char *elements[] = {"in_illuminance"};
    char datatypebuffer[ALS_DATATYPE_BUFSIZE+1] = {0};
    int fd, bufferfd = -1, ret = 0;
    char data;
    struct scanelement *element;

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

    scanelements.bufferdefaultvalue = data;

    if (data == '1') {
        data = '0';
        // Disable buffer so we can enable scan elements. Else, write operations will fail on them
        if (write(bufferfd, &data, sizeof(char)) == -1) {
            fprintf(stderr, "Error writing file %s\n", filepath);
            perror("");
            goto result;
        }
    }

    for (i = 0; i < sizeof(elements)/sizeof(elements[0]); i++) {
        memset(filepath, 0, sizeof(char[PATH_MAX]));
        strcat(filepath, path);
        strcat(filepath, "/scan_elements/");
        strcat(filepath, elements[i]);
        strcat(filepath, "_en");

        len = strlen(filepath);

        fd = open(filepath, O_RDWR);
        if (fd == -1) {
            perror("open() failed");
            fprintf(stderr, "Unable to open file for rw: %s\n", filepath);
        } else {
            element = alsmkscanelement(elements[i], '0', '0', fd);

            if (read(fd, &data, sizeof(char)) <= 0) {
                perror("read() failed");
            } else {
                element->defaultvalue = data;
                element->enabled = '1';
                if (data != '1') {
                    if (write(fd, &element->enabled, sizeof(char)) == -1) {
                        element->enabled = '0';
                        perror("write() failed");
                    }
                }
            }

            // Handle data type structure
            memcpy(&filepath[len-2], "type", sizeof(char[5]));
            fd = open(filepath, O_RDONLY);
            if (fd == -1) {
                fprintf(stderr, "Error reading file %s\n", filepath);
                perror("");
                goto result;
            }

            if (read(fd, datatypebuffer, sizeof(char[ALS_DATATYPE_BUFSIZE])) <= 0) {
                fprintf(stderr, "Error reading data type file %s", filepath);
                perror("");
                close(fd);
                goto result;
            }

            close(fd);

            if (parsedatatypebuffer(element, datatypebuffer) == 0) {
                fprintf(stderr, "Error parsing element data type\n");
                goto result;
            }
            
            if (alsaddscanelement(element) == 0) {
                fprintf(stderr, "Error adding scan element\n");
                goto result;
            }
        }
    }

    // Enable back buffer
    data = '1';
    if (write(bufferfd, &data, sizeof(char)) == -1) {
        perror("Error writing buffer file");
        fprintf(stderr, "Unable to enable iio buffers for als\n");
    }

    ret = 1;
    scanelements.bufferfd = bufferfd;

    result:
        if (ret == 0) {
            if (bufferfd >= 0) {
                close(bufferfd);
            }

            alsdestroyscanelements();
        }

        return ret;
}

void alsdestroydevice(void *self)
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

    result = mkdevice(AMBIENT_LIGHT_SENSOR, "als", 0, ALS_LOW_THRESHOLD, ALS_HIGH_THRESHOLD, path, NULL, NULL, alsdestroydevice);

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
    
    pthread_mutex_lock(&device->mutex);

    for (i = 0; i < scanelements.n; i++) {
        element = scanelements.elements[i];

        if (strcasecmp("in_illuminance", element->name) == 0) {
            uint8_t sz = element->datatype.bitsize/8;
            int buffer = 0;
            ssize_t n = 0;

            while ((n = read(fd, (&buffer)+n, sizeof(char[sz - n]))) > 0 && n != sz);

            if (n <= 0) {
                fprintf(stderr, "Reading of %s element in %s failed\n", element->name, device->path);
                perror("read() failed");
                continue;
            }

           
            buffer = buffer >> element->datatype.shifts;
            // buffer = buffer & (element->datatype.validbits/8);
            

            device->percentage = (100/(float) device->max_value) * (float) buffer;
            adjustdevices(device->percentage, dbuf);
            printf("Read: %lf\n", device->percentage);
        }
    }
    // printf("Modified: %s, fd: %d\n", device->path, fd);

    pthread_mutex_unlock(&device->mutex);

    // printf("Exited thread: %ld\n", pthread_self());
    return NULL;
}

int scanals(struct dbuf *dbuf, struct watcherbuf *watcherbuf)
{
    int ret = 0;
    struct dirent *dirent;
    char devfile[PATH_MAX] = {0};
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
        Device *device;

        if (devicefile == NULL) {
            fprintf(stderr, "Unable to determine als device");
            goto br;
        }

        device = loadalsdevice(devicefile);
        if (device != NULL) {
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
                
                    watch(watcherbuf, devfile, 0, alswatchcallback, device, NULL);
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
        return ret;
}