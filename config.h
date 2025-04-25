#ifndef _ALS_CONFIG_H
    #define _ALS_CONFIG_H

    #define ALS_CONFIG_LINE_MAX 128
    #define ALS_CONFIG_PATH "/etc/als-led-backlight.conf"
    #define ALS_CONFIG_COMMENT_CHAR '#'
    #define ALS_CONFIG_ASSIGN_CHAR '='

    #define ALS_DEFAULT_LOW_THRESHOLD 428
    #define ALS_DEFAULT_HIGH_THRESHOLD 10000

    #define ALS_VERBOSE_LEVEL_0 0
    #define ALS_VERBOSE_LEVEL_1 1
    #define ALS_VERBOSE_LEVEL_2 2

    typedef struct {
        uint16_t alshighthreshold;
        uint16_t alslowthreshold;
        char kbdpauseonmanualadjust;
        uint8_t verbose;
    } Config;

    int8_t configinit();
    Config getconfig();
#endif