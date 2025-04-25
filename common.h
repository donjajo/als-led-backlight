#ifndef _COMMON_H
    #define _COMMON_H

    char *strcat_(const char *src, const char *append);
    char *rtrim(char *str, char c);
    int setblocking(int fd, int status);
    char *strrchrr(const char *s, int c);
    char *ltrim(char *str, char c);
    char *trim(char *str, char c);
#endif