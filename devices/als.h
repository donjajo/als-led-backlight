#ifndef _ALS_H
    #define _ALS_H

    #define ALS_LOW_THRESHOLD 428
    #define ALS_HIGH_THRESHOLD 10000
    #define ALS_DATATYPE_BUFSIZE 128

    enum ElementDataTypEnum {
        BIGENDIAN,
        LITTLEENDIAN,
        SIGNEDINT,
        UNSIGNEDINT
    };

    struct elementdatatype {
        enum ElementDataTypEnum endianness;
        enum ElementDataTypEnum twoscompl;
        uint8_t validbits;
        uint8_t bitsize;
        uint8_t repeat;
        uint8_t shifts;
    };

    struct scanelement {
        char name[NAME_MAX];
        char enabled;
        char defaultvalue;
        int fd;
        struct elementdatatype datatype;
    };

    struct scanelements {
        struct scanelement **elements;
        size_t n;
        char bufferdefaultvalue;
        int bufferfd;
    };

    int scanals(struct dbuf *dbuf, struct watcherbuf *watcherbuf);
#endif