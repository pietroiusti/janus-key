#include "libevdev/libevdev-uinput.h"
#include <time.h>

// Modified key: key to which a primary and/or a secondary function
// has been assigned. (Those keys to which a secondary function has
// been assigned are called `janus keys`.)
typedef struct {
    unsigned int key;
    unsigned int primary_function;
    unsigned int secondary_function;
    unsigned int state;
    struct timespec last_time_down;
} mod_key;
