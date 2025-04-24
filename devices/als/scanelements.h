#ifndef _SCANELEMENTS_H
    #define _SCANELEMENTS_H

    #define ALS_DATATYPE_BUFSIZE 128

    #define SCANELEMENT_OPT_RDONLY 0x55

    enum ElementDataTypEnum {
        BIGENDIAN,
        LITTLEENDIAN,
        SIGNEDINT,
        UNSIGNEDINT
    };

    enum ScanElementType {
        UNKNOWN,
        IN_ILLUMINANCE
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
        enum ScanElementType type;
        char enabled;
        char defaultvalue;
        int fd;
        size_t index;
        pthread_mutex_t mutex;
        struct elementdatatype datatype;
    };

    struct scanelements {
        struct scanelement **elements;
        size_t n;
        char bufferdefaultvalue;
        int bufferfd;
        pthread_mutex_t mutex;
        size_t totalbits;
    };

    void alsdestroyscanelements();
    struct scanelements *alsgetscanelements();
    struct scanelement *alsmkscanelement(const char name[NAME_MAX], enum ScanElementType type, char enabled, char defaultvalue, int fd);
    uint8_t alsloadscanelements(const char *path);
#endif