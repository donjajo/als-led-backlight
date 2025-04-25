#ifndef _ALS_CONFIG_H
    #define _ALS_CONFIG_H

    #define ALS_CONFIG_LINE_MAX 128
    #define ALS_CONFIG_PATH "/etc/als-led-backlight.conf"
    #define ALS_CONFIG_COMMENT_CHAR '#'
    #define ALS_CONFIG_ASSIGN_CHAR '='

    #define ALS_DEFAULT_LOW_THRESHOLD 428
    #define ALS_DEFAULT_HIGH_THRESHOLD 10000

    typedef struct {
        uint16_t alshighthreshold;
        uint16_t alslowthreshold;
        char kbdpauseonmanualadjust;
    } Config;

    int8_t configinit();
    Config getconfig();
#endif