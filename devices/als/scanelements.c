#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>

#include "../../common.h"
#include "scanelements.h"
#include "../../watcher.h"
#include "../../config.h"

struct scanelements scanelements = {NULL, 0, '0', -1, PTHREAD_MUTEX_INITIALIZER, 0};

struct scanelements *alsgetscanelements()
{
    return &scanelements;
}

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
    pthread_mutex_lock(&element->mutex);

    if (element->fd > -1) {
        if (element->defaultvalue != element->enabled)
            write(element->fd, &element->defaultvalue, sizeof(char));

        close(element->fd);
    }

    pthread_mutex_unlock(&element->mutex);

    pthread_mutex_destroy(&element->mutex);

    free(element);
}

void alsdestroyscanelements()
{
    char data = '0';

    pthread_mutex_lock(&scanelements.mutex);

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

    pthread_mutex_unlock(&scanelements.mutex);

    pthread_mutex_destroy(&scanelements.mutex);
}

struct scanelement *alsmkscanelement(const char name[NAME_MAX], enum ScanElementType type, char enabled, char defaultvalue, int fd)
{
    struct scanelement *element = malloc(sizeof(struct scanelement));
    if (element == NULL) {
        return element;
    }

    element->enabled = enabled;
    element->defaultvalue = defaultvalue;
    element->fd = fd;
    element->type = type;
    element->datatype = (struct elementdatatype) {0, 0, 0, 0, 0, 0};
    memset(element->name, 0, sizeof(char[NAME_MAX]));
    strncpy(element->name, name, NAME_MAX);
    pthread_mutex_init(&element->mutex, NULL);

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

enum ScanElementType alsgetenum(const char *elementname)
{
    enum ScanElementType e = UNKNOWN;

    if (strcasecmp("in_illuminance", elementname) == 0) {
        e = IN_ILLUMINANCE;
    }

    return e;
}

struct scanelement *alsfindscanelement(const char *name)
{
    ssize_t i = scanelements.n;

    while (i-- && strcasecmp(scanelements.elements[i]->name, name) != 0);

    if (i == -1)
        return NULL;

    return scanelements.elements[i];
}

struct scanelement *alsfindscanelementbyfilename(const char *filename)
{
    char *elementname = strrchrr(filename, '_');
    struct scanelement *element;

    if (elementname == NULL)
        return NULL;

    element = alsfindscanelement(elementname);
    free(elementname);

    return element;
}

uint8_t alsloadscanelement_type(const char *filepath, struct scanelement *element)
{
    int fd = -1;
    uint8_t ret = 0;
    char datatypebuffer[ALS_DATATYPE_BUFSIZE+1] = {0};

    fd = open(filepath, O_RDONLY);

    if (fd == -1) {
        fprintf(stderr, "Error opening file %s: ", filepath);
        perror("open() failed");
        goto result;
    }

    if (read(fd, datatypebuffer, sizeof(char[ALS_DATATYPE_BUFSIZE])) <= 0) {
        fprintf(stderr, "Error reading data type file %s", filepath);
        perror("");
        goto result;
    }

    if (parsedatatypebuffer(element, datatypebuffer) == 0) {
        fprintf(stderr, "Error parsing element data type\n");
        goto result;
    }

    ret = 1;

    result:
    if (fd > -1)
        close(fd);

    return ret;
}

uint8_t alsloadscanelement_en(const char *filepath, struct scanelement *element, uint8_t rdonly)
{
    int fd = -1;
    uint8_t ret = 0;
    char data;

    if (!rdonly) {
        fd = open(filepath, O_RDWR);

        if (fd == -1) {
            fprintf(stderr, "Error opening file %s: ", filepath);
            perror("open() failed");
            goto result;
        }
    } else {
        if (lseek(element->fd, 0, SEEK_SET) == -1) {
            perror("lseek() failed");
            goto result;
        }

        fd = element->fd;
    }

    if (read(fd, &data, sizeof(char)) <= 0) {
        fprintf(stderr, "Error reading file %s: ", filepath);
        perror("read() failed");
        goto result;
    } 

    element->defaultvalue = data;
    element->enabled = data;

    if (!rdonly) {
        if (element->type == IN_ILLUMINANCE) {
            element->enabled = '1';
        } else {
            // Disable other scan_elements for now till we resolve why read of /dev/iio:device* fails when other elements are enabled
            element->enabled = '0';
        }

        if (data != element->enabled) {
            if (write(fd, &element->enabled, sizeof(char)) == -1) {
                fprintf(stderr, "Error enabling scan_element %s\n", filepath);
                perror("write() failed");
                goto result;
            }
        }
    }

    ret = 1;
    element->fd = fd;

    result:
    return ret;
}

uint8_t alsloadscanelement_index(const char *filepath, struct scanelement *element)
{
    int fd = -1;
    uint8_t ret = 0;
    char data = 0;
    ssize_t n;

    fd = open(filepath, O_RDONLY);

    if (fd == -1) {
        fprintf(stderr, "Error opening file %s: ", filepath);
        perror("open() failed");
        goto result;
    }
    
    n = read(fd, &data, sizeof(char));

    if (n == -1) {
        fprintf(stderr, "Error reading file %s", filepath);
        perror("");

        goto result;
    }

    element->index = data - '0';

    ret = 1;

    result:
    if (fd != -1)
        close(fd);

    return ret;
}

int alsloadscanelementdatafromfile(const char *filepath, struct scanelement *element, uint8_t options)
{
    char *prop;
    int ret = 0;
    uint8_t mask = 0x0f;

    pthread_mutex_lock(&element->mutex);

    prop = strrchr(filepath, '_');
    if (prop == NULL) {
        goto result;
    }

    if (strcasecmp(prop, "_type") == 0) {
        ret = alsloadscanelement_type(filepath, element);
    } else if (strcasecmp(prop, "_en") == 0) {
        ret = alsloadscanelement_en(filepath, element, mask & options);
    } else if (strcasecmp(prop, "_index") == 0) {
        ret = alsloadscanelement_index(filepath, element);
    } else {
        ret = -1;
    }

    result:
    pthread_mutex_unlock(&element->mutex);
    return ret;
}

void alsrecalculatetotalbits()
{
    size_t i;

    scanelements.totalbits = 0;
    for (i = 0; i < scanelements.n; i++) {
        if (scanelements.elements[i]->enabled == '1')
            scanelements.totalbits += scanelements.elements[i]->datatype.bitsize;
    }
}

void *alsscanelementswatchcallback(void *a)
{
    struct watcherthreadargs *args = (struct watcherthreadargs *) a;
    struct inotify_event *evt = args->evt;
    struct scanelement *element;

    if ((element = alsfindscanelementbyfilename(evt->name)) != NULL) {
        if (!alsloadscanelementdatafromfile(evt->name, element, SCANELEMENT_OPT_RDONLY)) {
            fprintf(stderr, "Error updating scan_elements from events %s\n", evt->name);
        } else {
            printf("Element file %s of %s updated\n", evt->name, element->name);
        }

        pthread_mutex_lock(&scanelements.mutex);
        alsrecalculatetotalbits();
        pthread_mutex_unlock(&scanelements.mutex);
    }

    return NULL;
}

uint8_t alsloadscanelements(const char *path)
{
    char filepath[PATH_MAX] = {0};
    uint8_t ret = 0;
    struct scanelement *element = NULL;
    DIR *dir;
    struct dirent *direntry;
    char *elementname = NULL;
    enum ScanElementType elementtype;
    size_t j;
    Config config  = getconfig();

    strcat(filepath, path);
    strcat(filepath, "/scan_elements/");
    j = strnlen(filepath, PATH_MAX);

    char scanelementsdir[j+1];
    strncpy(scanelementsdir, filepath, j);
    scanelementsdir[j] = 0;

    dir = opendir(filepath);
    if (dir == NULL) {
        fprintf(stderr, "Error reading scan_elements directory %s", filepath);
        perror("");

        goto result;
    }

    pthread_mutex_lock(&scanelements.mutex);

    while ((direntry = readdir(dir)) != NULL) {
        if (direntry->d_type == DT_REG) {
            memset(filepath, 0, sizeof(char[PATH_MAX]));
            strcat(filepath, path);
            strcat(filepath, "/scan_elements/");
            strcat(filepath, direntry->d_name);

            elementname = strrchrr(direntry->d_name, '_');

            if (elementname == NULL) {
                fprintf(stderr, "Error determining element name: %s\n", direntry->d_name);
                goto result;
            }

            elementtype = alsgetenum(elementname);

            if ((element = alsfindscanelement(elementname)) == NULL) {
                element = alsmkscanelement(elementname, elementtype, '0', '0', -1);

                if (element == NULL) {
                    fprintf(stderr, "Unable to make element\n");
                    goto result;
                }

                if (alsaddscanelement(element) == 0) {
                    fprintf(stderr, "Error adding scan element\n");
    
                    goto result;
                }
            }

            if (alsloadscanelementdatafromfile(filepath, element, 0) == 0) {
                goto result;
            }

            free(elementname);
            elementname = NULL;
        }
    }

    if (config.verbose >= ALS_VERBOSE_LEVEL_2) {
        for (size_t i = 0; i < scanelements.n; i++) {
            element = scanelements.elements[i];
            printf("Element: %d\n", element->type);
            printf("Index: %ld\n", element->index);
            printf("Enabled: %c\n", element->defaultvalue);
            printf("Endian: %d\nSigned: %d\nvalidbits: %u\ntotalbits: %u\nRepeat:%u\nshift: %u\n", element->datatype.endianness, element->datatype.twoscompl, element->datatype.validbits, element->datatype.bitsize, element->datatype.repeat, element->datatype.shifts);
            printf("\n");
        }
    }

    if (watch(scanelementsdir, IN_MODIFY, alsscanelementswatchcallback, NULL, NULL) == 0) {
        fprintf(stderr, "Error watching scan_elements files\n");
        goto result;
    }

    alsrecalculatetotalbits();

    ret = 1;

    result:
    if (dir != NULL)
        closedir(dir);

    if (elementname != NULL)
        free(elementname);

    pthread_mutex_unlock(&scanelements.mutex);

    return ret;
}