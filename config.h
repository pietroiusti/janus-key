#include "libevdev/libevdev-uinput.h"
#include <time.h>

typedef struct {
    unsigned int key1;
    unsigned int key2;
} key_pair;

// keys' primary function map
key_pair pf_map[] = {
    // {KEY_A, KEY_B},
    {KEY_CAPSLOCK, KEY_ESC},
};

typedef struct {
    unsigned int key1;
    unsigned int key2;
    unsigned int state;
    struct timespec last_time_down;
} janus_key;

// keys' secondary function map
janus_key janus_map[] = {
    // assuming x keycodes are left default, this works for my filco usb keyboard
    {KEY_CAPSLOCK, KEY_LEFTALT},
    {KEY_ENTER, KEY_RIGHTALT},
    {KEY_LEFTCTRL, KEY_LEFTMETA},
    {KEY_LEFTMETA, KEY_LEFTALT},
    {KEY_LEFTALT, KEY_LEFTCTRL},
    {KEY_RIGHTALT, KEY_RIGHTCTRL},
    {KEY_RIGHTMETA, KEY_RIGHTALT},
    {KEY_COMPOSE, KEY_RIGHTMETA},
};

// Delay in milliseconds
double max_delay = 300;
