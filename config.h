#include "libevdev/libevdev-uinput.h"
#include <time.h>

typedef struct {
    unsigned int key;
    unsigned int secondary_function;
    unsigned int state;
    struct timespec last_time_down;
} janus_key;

janus_key janus_map[] = {
    // Key                   Secondary Function
    {  KEY_CAPSLOCK,         KEY_LEFTMETA    },
    {  KEY_ENTER,            KEY_RIGHTMETA   },
};

// Delay in milliseconds
double max_delay = 300;
