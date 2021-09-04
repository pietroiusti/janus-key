#include "libevdev/libevdev-uinput.h"

typedef struct {
    unsigned int key1;
    unsigned int key2;
} janus_key;

janus_key janus_map[2] = {
    {KEY_CAPSLOCK, KEY_LEFTMETA},
    {KEY_ENTER, KEY_RIGHTMETA},
};

// Delay in milliseconds
double max_delay = 300;
