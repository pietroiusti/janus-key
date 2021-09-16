#include "libevdev/libevdev-uinput.h"
#include <time.h>

typedef struct {
    unsigned int key;
    unsigned int primary_function;
    unsigned int secondary_function;
    unsigned int state;
    struct timespec last_time_down;
} janus_key;

janus_key janus_map[] = {
    {  KEY_CAPSLOCK,  KEY_ESC,        KEY_LEFTALT     },
    {  KEY_ENTER,     KEY_ENTER,      KEY_RIGHTALT    },
    /* {  KEY_SPACE,     KEY_SPACE,      KEY_RIGHTALT    }, */
    {  KEY_RIGHTALT,  KEY_RIGHTCTRL },
    {  KEY_LEFTALT,   KEY_LEFTCTRL  },
    {  KEY_LEFTMETA,  KEY_LEFTALT   },
    {  KEY_RIGHTMETA, KEY_RIGHTALT  },
    {  KEY_LEFTCTRL,  KEY_LEFTMETA  },
    {  KEY_COMPOSE,   KEY_RIGHTMETA },
    {  KEY_ESC,       KEY_CAPSLOCK  }
};

// Delay in milliseconds.
// If a key is held down for a time > max_delay, then, when released,
// it will not act in the standard way.
unsigned int max_delay = 300;
