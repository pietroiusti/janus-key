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

// Delay in milliseconds.
// If a key is held down for a time > max_delay, then, when released,
// it will not act in the standard way.
unsigned int max_delay = 300;
