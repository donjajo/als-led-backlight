#include <sys/inotify.h>

#ifndef _WATCHER_H
    #define _WATCHER_H

    struct watcherthreadargs {
        void *evt;
        void *metadata;
        struct dbuf *dbuf;
    };

    struct watcherthread {
        int fd;
        pthread_t tid;
    };

    struct watcher {
        int fd;
        uint8_t isinotify;
        void *(*callback)(void *args);
        struct watcher *next;
        void *metadata;
        void (*destorycallback)(struct watcher *watcher);
    };

    struct watcherbuf {
        int fd;
        struct watcher **watchers;
        size_t c;
        pthread_mutex_t mutex;
    };

    struct watcherbuf *mkwatcherbuffer();
    int watch(struct watcherbuf *watcherbuf, const char *pathname, uint32_t mask, void *(*callback)(void *args), void *metadata, void (*destorycallback)(struct watcher *watcher));
    void initwatcher(struct watcherbuf *buf, struct dbuf *dbuf);
    void destroywatcherbuffer(struct watcherbuf *watcherbuf);
#endif