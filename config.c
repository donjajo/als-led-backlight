#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>

#include "config.h"
#include "common.h"

Config config;

int32_t loadconfigline(const char *key, const char *value, size_t ln)
{
    int32_t ret = 0;

    if (strcasecmp(key, "als_high_threshold") == 0) {
        config.alshighthreshold = atoi(value);
        ret = 1;
    } else if (strcasecmp(key, "als_low_threshold") == 0) {
        config.alslowthreshold = atoi(value);
        ret = 1;
    } else if (strcasecmp(key, "kbd_pause_on_manual_adjust") == 0) {
        if (value[0] != '1' && value[0] != '0') {
            fprintf(stderr, "Invalid config value for %s. Expected 1 or 0\n", key);

            goto result;
        }

        config.kbdpauseonmanualadjust = value[0];
        ret = 1;
    } else {
        fprintf(stderr, "Unknown config key %s on line %ld\n", key, ln);
        ret = -1;
    }

    result:
    return ret;
}

Config getconfig()
{
    return config;
}

ssize_t parseconfigline(char line[ALS_CONFIG_LINE_MAX], ssize_t n)
{
    static size_t ln = 0;
    ssize_t i = 0;
    char *key = NULL, *value = NULL;

    ln++;

    for (; i < n; i++) {
        if (line[i] == '\n') {
            line[i] = 0;
            break;
        }
    }

    if (line[0] != ALS_CONFIG_COMMENT_CHAR) {
        key = strrchrr(line, ALS_CONFIG_ASSIGN_CHAR);
        if (key == NULL) {
            fprintf(stderr, "Error parsing config file on line %ld\n", ln);
            i = -1;
            goto result;
        }

        rtrim(key, ' ');

        value = strrchr(line, ALS_CONFIG_ASSIGN_CHAR);
        // Skip the = sign
        value = trim(&value[1], ' ');
        
        if (loadconfigline(key, value, ln) != 1) {
            i = -1;
        }
    }

    result:
    if (key != NULL)
        free(key);
    return i;
}

int8_t configinit()
{
    int fd = -1;
    ssize_t n, j, pos = 0;
    int8_t ret = 0;
    char linebuf[ALS_CONFIG_LINE_MAX+1] = {0};

    config.alshighthreshold = ALS_DEFAULT_HIGH_THRESHOLD;
    config.alslowthreshold = ALS_DEFAULT_LOW_THRESHOLD;
    config.kbdpauseonmanualadjust = 0;

    if (access(ALS_CONFIG_PATH, F_OK) == 0) {
        fd = open(ALS_CONFIG_PATH, O_RDONLY);

        if (fd == -1) {
            fprintf(stderr, "Error opening config file %s", ALS_CONFIG_PATH);
            perror("");

            goto result;
        }

        while ((n = read(fd, linebuf, sizeof(char[ALS_CONFIG_LINE_MAX]))) > 0) {
            j = parseconfigline(linebuf, n);
            if (j == -1) {
                goto result;
            }

        
            pos += 1 + j;
            // printf("n: %ld, j: %ld, pos: %ld\n", n , j, pos);
            lseek(fd, pos, SEEK_SET);

            memset(linebuf, 0, sizeof(char[ALS_CONFIG_LINE_MAX]));
        }
    }

    ret = 1;

    result:
    if (fd != -1) {
        close(fd);
    }

    return ret;
}