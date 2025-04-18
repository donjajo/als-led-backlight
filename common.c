#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "common.h"

char *strcat_(const char *src, const char *append)
{
    size_t srclen = strlen(src);
    size_t appendlen = strlen(append);

    char *ret = calloc(srclen + appendlen + 1, sizeof(char));
    if (ret == NULL) {
        return NULL;
    }

    strcat(ret, src);
    strcat(ret, append);

    return ret;
}

char *rtrim(char *str, char c)
{
    size_t len = strlen(str);
    for (
        size_t i = len-1;
        i > 0;
        i--
    ) {

        if (str[i] != c) {
            break;
        } else {
            str[i] = 0;
        }
    }

    return str;
}


int setblocking(int fd, int status)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (status) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }

    return fcntl(fd, F_SETFL, flags);
}