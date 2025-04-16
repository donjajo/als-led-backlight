#include <stdio.h>
#include <signal.h>

#include "common.h"
#include "devices.h"
#include "watcher.h"

struct dbuf *devicesbuffer = NULL;
struct watcherbuf *watcherbuffer = NULL;
pthread_mutex_t exitmutex;
uint8_t exited = 0;

void exitgracefully()
{
    pthread_mutex_lock(&exitmutex);

    if (exited)
        return;

    printf("Exiting\n");
    if (watcherbuffer != NULL) {
        destroywatcherbuffer(watcherbuffer);
    }

    if (devicesbuffer != NULL)
        destorydbuf(devicesbuffer);
    
    exited = 1;
    pthread_mutex_unlock(&exitmutex);
}

void sighandler(int sig)
{
    exitgracefully();
}

int main()
{
    struct sigaction sa;
    int ret = 1;

    pthread_mutex_init(&exitmutex, NULL);

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sighandler;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction() failed");
        goto close;
    }

    devicesbuffer = mkdbuf();
    if (devicesbuffer == NULL) {
        fprintf(stderr, "Unable to create buffer space");

        goto close;
    }

    watcherbuffer = mkwatcherbuffer();
    if (watcherbuffer == NULL) {
        fprintf(stderr, "Error creating watcher buffer\n");

        goto close;
    }

    printf("Ambient Light Sensor LED Backlight app started\n");
    printf("Scanning available devices...\n");

    if (!scandevices(KEYBOARD_BACKLIGHT, devicesbuffer, watcherbuffer)) {
        fprintf(stderr, "Error detecting keyboard light\n");

        goto close;
    }

    if (!scandevices(AMBIENT_LIGHT_SENSOR, devicesbuffer, watcherbuffer)) {
        fprintf(stderr, "Error detecting ambient light sensor device\n");

        goto close;
    }

    ret = 0;

    initwatcher(watcherbuffer, devicesbuffer);

    close:
        exitgracefully();

    return ret;
}