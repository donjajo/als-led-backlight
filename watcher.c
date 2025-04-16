#include <stdio.h>
#include <sys/inotify.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>

#include "watcher.h"

void destorywatcher(struct watcher *watcher, struct watcherbuf *watcherbuf)
{
    if (watcher->destorycallback != NULL)
        watcher->destorycallback(watcher);

    if (watcher->isinotify)
        inotify_rm_watch(watcherbuf->fd, watcher->fd);
    else
        close(watcher->fd);

    free(watcher);
}

void destroywatcherbuffer(struct watcherbuf *watcherbuf)
{
    struct watcher *watcher;
    struct watcher *t;

    pthread_mutex_lock(&watcherbuf->mutex);

    while (watcherbuf->c--) {
        watcher = watcherbuf->watchers[watcherbuf->c];
        while (watcher->next != NULL) {
            t = watcher->next;
            destorywatcher(watcher, watcherbuf);
            watcher = t;
        }

        destorywatcher(watcher, watcherbuf);
    }

    if (watcherbuf->watchers != NULL)
        free(watcherbuf->watchers);

    pthread_mutex_unlock(&watcherbuf->mutex);
    pthread_mutex_destroy(&watcherbuf->mutex);
    free(watcherbuf);
}

struct watcherbuf *mkwatcherbuffer()
{
    struct watcherbuf *buf = malloc(sizeof(struct watcherbuf));
    if (buf == NULL) {
        perror("malloc() failed");
        return NULL;
    }

    buf->fd = inotify_init();
    if (buf->fd < 0) {
        perror("inotify_init() failed");

        goto close;
    }
    
    buf->watchers = NULL;
    buf->c = 0;
    if (pthread_mutex_init(&buf->mutex, NULL) != 0) {
        perror("pthread_mutex_init() failed");

        goto close;
    }

    return buf;

    close:
        if (buf != NULL)
            destroywatcherbuffer(buf);

        return NULL;
}

struct watcher *findwatcher(struct watcherbuf *buf, uint32_t wd)
{
    size_t i = wd % buf->c;
    struct watcher *watcher = buf->watchers[i];

    while (watcher->fd != wd && watcher->next != NULL)
        watcher = watcher->next;

    return watcher;
}

void *processevent(void *targs)
{
    pthread_detach(pthread_self());
    // sigset_t signal_set;
    // sigemptyset(&signal_set);
    // sigaddset(&signal_set, SIGINT);
    // pthread_sigmask(SIG_BLOCK, &signal_set, NULL);

    struct watcherthreadargs *a = (struct watcherthreadargs *) targs;

    a->watcher->callback(a);

    // free(targs);
    return NULL;
}

void threadcleanup(struct watcherthread *thread)
{
    pthread_join(thread->tid, NULL);
    thread->tid = 0;

    if (thread->args != NULL)
        free(thread->args);

    thread->args = NULL;
}

/**
 * @brief This inits the watcher. 
 * NB: Actually, this would have worked perfectly without threads. But I realised that /dev/iio* device
 * blocks read sometimes, hence, it affects events of other devices. But once data is available, it reads.
 * To prevent other devices' event from blocking others, I adopted threads for that. But each device must only
 * have one thread at a time so we handle cleanup properly.
 * 
 * @param buf 
 * @param dbuf 
 */
void initwatcher(struct watcherbuf *buf, struct dbuf *dbuf)
{
    ssize_t n;
    size_t len = sizeof(struct inotify_event) + NAME_MAX + 1;
    char buffer[len];
    char *p;
    struct inotify_event *evt;
    struct watcher *watcher;
    fd_set readfds;
    int watchfds[5];
    struct watcherthreadargs *targs = NULL;
    size_t i, j = 0;

    // Since we both need to watch inotify events and other descriptors for new data, we are using select to group all of them
    FD_ZERO(&readfds);
    FD_SET(buf->fd, &readfds);
    watchfds[j++] = buf->fd;

    for (i = 0; i < buf->c; i++) {
        watcher = buf->watchers[i];

        if (watcher->isinotify == 0) {
            FD_SET(watcher->fd, &readfds);
            watchfds[i] = watcher->fd;
            j++;
        }

        while ((watcher = watcher->next) != NULL) {
            if (watcher->isinotify == 0) {
                FD_SET(watcher->fd, &readfds);
                watchfds[j++] = watcher->fd;
            }
        }
    }

    // Set thread containers
    struct watcherthread threads[j];
    for (i = 0; i < j; i++) {
        threads[i] = (struct watcherthread) {watchfds[i], 0, NULL};
    }
    
    while (1) {
        n = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);

        if (n == -1) {
            perror("read() error");
            break;
        }

        for (i = 0; i < j; i++) {
            if (FD_ISSET(watchfds[i], &readfds)) {
                targs = malloc(sizeof(struct watcherthreadargs));

                n = read(watchfds[i], buffer, len);

                if (n == 0) {
                    perror("read returned 0!");
                    break;
                }

                if (n == -1) {
                    perror("read() error");
                    break;
                }

                pthread_mutex_lock(&buf->mutex);

                if (watchfds[i] == buf->fd) {
                    for (p = buffer; p < buffer + n;) {
                        evt = (struct inotify_event *) p;
            
                        watcher = findwatcher(buf, evt->wd);
                        targs->evt = evt;
                        targs->metadata = watcher->metadata;
                        targs->dbuf = dbuf;
                        targs->watcher = watcher;

                        if (threads[i].tid > 0)
                            threadcleanup(&threads[i]);

                        pthread_create(&threads[i].tid, NULL, processevent, targs);
                        threads[i].args = targs;
                        // printf("Started thread~~: %ld\n", threads[i].tid);

                        p += sizeof(struct inotify_event) + evt->len;
                    }
                } else {
                    watcher = findwatcher(buf, watchfds[i]);
                    targs->evt = &watchfds[i];
                    targs->metadata = watcher->metadata;
                    targs->dbuf = dbuf;
                    targs->watcher = watcher;

                    // Can only have thread at a time. Else, we wait for it
                    if (threads[i].tid > 0)
                            threadcleanup(&threads[i]);

                    pthread_create(&threads[i].tid, NULL, processevent, targs);
                    threads[i].args = targs;
                    // printf("Started thread: %ld\n", threads[i].tid);
                }

                pthread_mutex_unlock(&buf->mutex);
            } else {
                FD_SET(watchfds[i], &readfds);
            }
        }
    }

    for (i = 0; i < j; i++) {
        if (threads[i].tid > 0) {
            threadcleanup(&threads[i]);
        }
    }
}

/**
 * @brief Watch a filesystem object. Since some devices do not support inotify, we can watch them using select.
 * 
 * @param watcherbuf 
 * @param pathname 
 * @param mask      Providing 0 here means do not use inotify
 * @param callback 
 * @param metadata 
 * @param destorycallback 
 * @return int 
 */
int watch(struct watcherbuf *watcherbuf, const char *pathname, uint32_t mask, void *(*callback)(void *args), void *metadata, void (*destorycallback)(struct watcher *watcher))
{
    int ret = 0, fd, nextindex;
    struct watcher **watchers;
    struct watcher *current;
    uint8_t isinotify = mask != 0;

    pthread_mutex_lock(&watcherbuf->mutex);

    struct watcher *watcher = malloc(sizeof(struct watcher));
    if (watcher == NULL) {
        perror("malloc() failed");
        
        goto result;
    }

    if (isinotify) {
        // We treat as inotify watch
        fd = inotify_add_watch(watcherbuf->fd, pathname, mask);

        if (fd < 0) {
            perror("inotify_add_watch() failed");

            goto result;
        }
    } else {
        fd = open(pathname, O_RDONLY);
        if (fd < 0) {
            perror("open() failed");

            goto result;
        }
    }

    watcher->fd = fd;
    watcher->isinotify = isinotify;
    watcher->callback = callback;
    watcher->metadata = metadata;
    watcher->destorycallback = destorycallback;
    watcher->next = NULL;

    watchers = realloc(watcherbuf->watchers, sizeof(struct watcher *[watcherbuf->c+1]));
    if (watchers == NULL) {
        perror("realloc() failed");

        goto result;
    }
    
    nextindex = fd % (watcherbuf->c+1);
    watcherbuf->watchers = watchers;

    if (nextindex > watcherbuf->c-1 || watcherbuf->c == 0) {
        watcherbuf->watchers[nextindex] = watcher;
        watcherbuf->c++;
    } else {
        current = watcherbuf->watchers[nextindex];
        while (current->next != NULL) {
            current = current->next;
        }

        current->next = watcher;
    }

    ret = 1;
    // printf("Added watcher for %s; %d\n", pathname, fd);
    // for (size_t i = 0; i < watcherbuf->c; i++) {
    //     printf("Found %ld\n", i+1);
    //     struct watcher *w = watcherbuf->watchers[i];
    //     while (w->next != NULL) {
    //         printf("Found another\n");
    //         w = w->next;
    //     }
    // }

    result:
        if (ret == 0) {
            if (watcher != NULL)
                free(watcher);

            if (fd >= 0) {
                if (isinotify)
                    inotify_rm_watch(watcherbuf->fd, fd);
                else
                    close(fd);
            }
        }

        pthread_mutex_unlock(&watcherbuf->mutex);

        return ret;
}