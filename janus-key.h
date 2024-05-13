#include "libevdev/libevdev-uinput.h"
#include <time.h>

// Modified key: key to which a primary and/or a secondary function
// has been assigned. (Those keys to which a secondary function has
// been assigned are called `janus keys`.)
typedef struct {
    unsigned int key;
    unsigned int primary_function;
    unsigned int secondary_function;
    unsigned int state; // physical state of `key`. (= the last value received)
    unsigned int last_secondary_function_value_sent;
    unsigned int delayed_down;//whether delayed remapping should happen
    struct timespec send_down_at;//time at which delayed remapping should happen
    struct timespec last_time_down;
} mod_key;
