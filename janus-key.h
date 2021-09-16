#include "libevdev/libevdev-uinput.h"
#include <time.h>

typedef struct {
    unsigned int key;
    unsigned int primary_function;
    unsigned int secondary_function;
    unsigned int state;
    struct timespec last_time_down;
} janus_key;
