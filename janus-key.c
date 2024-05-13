/*

    janus-key. Give keys a double function.

    Copyright (C) 2021, 2022, 2023  Giulio Pietroiusti

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

// `janus keys`: those keys that have beeen assigned a secondary
//  function.

#include "./config.h"
#include <assert.h>
#include <bits/time.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include "libevdev/libevdev.h"
#include "libevdev/libevdev-uinput.h"
#include "poll.h"

// max delay set by user stored as a timespec struct
struct timespec delay_timespec;

int last_input_was_special_combination = 0;

// store delay value given by user into timespec struct
struct timespec tp_max_delay;

// For calculating delay
struct timespec now;
struct timespec tp_sum;

// If any of the janus keys is down or held return the index of the
// first one of them in the mod_map. Otherwise, return -1.
static int some_jk_are_down_or_held() {
    size_t length = sizeof(mod_map)/sizeof(mod_map[0]);
    for (int i = 0; i < length; i++) {
        if (mod_map[i].state == 1 || mod_map[i].state == 2 && mod_map[i].secondary_function > 0)
            return i;
    }
    return -1;
}

// If `key` is in the mod_map, then return its index. Otherwise return
// -1.
static int is_in_mod_map(unsigned int key) {
    size_t length = sizeof(mod_map)/sizeof(mod_map[0]);
    for (int i = 0; i < length; i++) {
        if (mod_map[i].key == key)
            return i;
    }
    return -1;
};

// If `key` is a janus key, then return its index. Otherwise return
// -1.
static int is_janus(unsigned int key) {
    int i = is_in_mod_map(key);
    if (i >= 0)
        if (mod_map[i].secondary_function > 0)
            return i;
    return -1;
}

// Compare two timespec structs.
// Return -1 if *tp1 > *tp2, 0 if *tp1 == *tp2, 1 if *tp1 < *tp2
static int timespec_cmp(struct timespec *tp1, struct timespec *tp2) {
    if (tp1->tv_sec > tp2->tv_sec) {
        return -1;
    }
    else if (tp1->tv_sec < tp2->tv_sec) {
        return 1;
    } else { // tp1->tv_sec == tp2->tv_sec
        if (tp1->tv_nsec > tp2->tv_nsec)
            return -1;
        else if (tp1->tv_nsec < tp2->tv_nsec)
            return 1;
        else
            return 0;
    }
}

// Add two timespec structs.
static void timespec_add(struct timespec* a, struct timespec* b, struct timespec* c) {
    if (a == NULL || b == NULL || c == NULL) {
        fprintf(stderr, "Invalid pointer in timespec_add\n");
        return;
    }

    c->tv_sec = a->tv_sec + b->tv_sec;
    c->tv_nsec = a->tv_nsec + b->tv_nsec;
    if (c->tv_nsec >= 1000000000) { // overflow
        c->tv_nsec -= 1000000000;
        c->tv_sec++;
    }
}

// Subtracts timespec b from timespec a and stores the result in timespec c.
// Assumes a >= b.
static void timespec_subtract(const struct timespec* a, const struct timespec* b, struct timespec* c) {
    if (a == NULL || b == NULL || c == NULL) {
        fprintf(stderr, "Invalid pointer in timespec_subtract\n");
        return;
    }

    c->tv_sec = a->tv_sec - b->tv_sec;
    c->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (c->tv_nsec < 0) { // Underflow
        c->tv_nsec += 1000000000;
        c->tv_sec--;
    }
}

// Converts a timespec struct to milliseconds.
static long timespec_to_ms(const struct timespec *ts) {
    if (ts == NULL) {
        fprintf(stderr, "Invalid pointer in timespec_to_ms\n");
        return 0; // Consider appropriate error handling or a different return value.
    }

    long milliseconds = ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
    return milliseconds;
}

// Converts milliseconds to a timespec struct.
static void ms_to_timespec(long ms, struct timespec *ts) {
    if (ts == NULL) {
        fprintf(stderr, "Invalid pointer in ms_to_timespec\n");
        return;
    }

    ts->tv_sec = ms / 1000; // Convert milliseconds to seconds
    ts->tv_nsec = (ms % 1000) * 1000000; // Convert the remainder to nanoseconds
}

// Post the EV_KEY event of code `code` and `value` through the
// uinput device `*uidev` and send a (EV_SYN, SYN_REPORT, 0) event
// through that same device.
static void send_key_ev_and_sync(const struct libevdev_uinput *uidev, unsigned int code, int value)
{
    int err;
    err = libevdev_uinput_write_event(uidev, EV_KEY, code, value);
    if (err != 0) {
        perror("Error in writing EV_KEY event\n");
        exit(err);
    }
    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
    if (err != 0) {
        perror("Error in writing EV_SYN, SYN_REPORT, 0.\n");
        exit(err);
    }
    printf("Sending %u %u\n", code, value);
}

// For each janus key down or held send an EV_KEY event with its
// secondary function code and value `value`.
static void send_down_or_held_jks_secondary_function(const struct libevdev_uinput *uidev, int value) {
    for (int i = 0; i < sizeof(mod_map)/sizeof(mod_map[0]); i++) {
        if ((mod_map[i].state == 1 || mod_map[i].state == 2) && mod_map[i].secondary_function > 0)  {
            mod_map[i].delayed_down = 0;
            if (mod_map[i].last_secondary_function_value_sent != value) {
                send_key_ev_and_sync(uidev, mod_map[i].secondary_function, value);
            } else printf("refraining from sending %d %d\n", mod_map[i].secondary_function, value);
            mod_map[i].last_secondary_function_value_sent = value;
        }
    }
}

// Post through uidev, with value `value, the primary function
// associated of `code`.
static void send_primary_function(const struct libevdev_uinput *uidev, unsigned int code, int value) {
    int i = is_in_mod_map(code);
    if (i >= 0) {
        unsigned int primary_function = mod_map[i].primary_function > 0 ? mod_map[i].primary_function : mod_map[i].key;
        send_key_ev_and_sync(uidev, primary_function, value);
    } else {
        send_key_ev_and_sync(uidev, code, value);
    }
}

static void handle_ev_key(const struct libevdev_uinput *uidev, unsigned int code, int value) {
    int jk_index = is_janus(code);
    if (jk_index >= 0) {
        mod_key *jk = &mod_map[jk_index];
        if (value == 1) {
            jk->state = 1;
            clock_gettime(CLOCK_MONOTONIC, &jk->last_time_down);
            last_input_was_special_combination = 0;

            // ###########################################
            //printf("SPECIAL BEHAVIOUR REQUIRED!\n");

            // calculate time in which the delayed down event should happen (if relevant conditions are fulfilled)
            struct timespec scheduled_delayed_down;
            /* printf("&jk->last_time_down.tv-sec: %lu\n", jk->last_time_down.tv_sec); */
            /* printf("&jk->last_time_down.tv-nsec: %lu\n", jk->last_time_down.tv_nsec); */
            timespec_add(&jk->last_time_down, &delay_timespec, &scheduled_delayed_down);
            /* printf("scheduled_delayed_down.tv_sec: %lu\n", scheduled_delayed_down.tv_sec); */
            /* printf("scheduled_delayed_down.tv_nsec: %lu\n", scheduled_delayed_down.tv_nsec); */

            jk->send_down_at = scheduled_delayed_down;
            /* printf("jk->send_down_at.tv_sec: %lu\n", jk->send_down_at.tv_sec); */
            /* printf("jk->send_down_at.tv_nsec: %lu\n", jk->send_down_at.tv_nsec); */
            jk->delayed_down = 1;
            // ###########################################
        } else if (value == 2) {
            jk->state = 2;
            last_input_was_special_combination = 0;
        } else {
            // ###########################################
            jk->delayed_down = 0;
            // ###########################################
            jk->state = 0;
            clock_gettime(CLOCK_MONOTONIC, &now);
            timespec_add(&jk->last_time_down, &tp_max_delay, &tp_sum);
            if (timespec_cmp(&now, &tp_sum) == 1) { // is considered as tap
                if (last_input_was_special_combination) {
                    if (jk->last_secondary_function_value_sent != 0) {
                        send_key_ev_and_sync(uidev, jk->secondary_function, 0);
                    } else printf("refraining from sending %d %d\n", jk->secondary_function, 0);
                    jk->last_secondary_function_value_sent = 0;
                } else {
                    if (some_jk_are_down_or_held() >= 0) {
                        last_input_was_special_combination = 1;
                        send_down_or_held_jks_secondary_function(uidev, 1);
                    } else {
                        send_down_or_held_jks_secondary_function(uidev, 0);
                    }
                    send_primary_function(uidev, jk->key, 1);
                    send_primary_function(uidev, jk->key, 0);
                }
            } else { // is not considered as tap
                if (jk->last_secondary_function_value_sent != 0) {
                    send_key_ev_and_sync(uidev, jk->secondary_function, 0);
                } else printf("refraining from sending %d %d\n", jk->secondary_function, 0);
                jk->last_secondary_function_value_sent = 0;
            }
        }
    } else {
        if (value == 1) {
            if (some_jk_are_down_or_held() >= 0) {
                last_input_was_special_combination = 1;
                send_down_or_held_jks_secondary_function(uidev, 1);
                send_primary_function(uidev, code, 1);
            } else {
                last_input_was_special_combination = 0;
                send_primary_function(uidev, code, 1);
            }
        } else if (value == 2) {
            if (some_jk_are_down_or_held() >= 0) {
                last_input_was_special_combination = 1;
                send_down_or_held_jks_secondary_function(uidev, 1);
                send_primary_function(uidev, code, 2);
            } else {
                last_input_was_special_combination = 0;
                send_primary_function(uidev, code, 2);
            }
        } else { // if (value == 0)
            send_primary_function(uidev, code, 0);
        }
    }
}

int main(int argc, char **argv)
{
    ms_to_timespec(max_delay, &delay_timespec);
    /* printf("delay_timespec.tv_sec: %lu\n", delay_timespec.tv_sec); */
    /* printf("delay_timespec.tv_nsec: %lu\n", delay_timespec.tv_nsec); */
    /* printf("In in ms: %lu\n", timespec_to_ms(&delay_timespec)); */

    tp_max_delay.tv_sec = 0;
    tp_max_delay.tv_nsec = max_delay * 1000000;

    struct libevdev *dev = NULL;
    const char *file;
    int fd;
    int rc = 1;

    if (argc < 2)
        goto out;

    usleep(100000); // let (KEY_ENTER), value 0 go through before

    file = argv[1];
    fd = open(file, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device\n");
        goto out;
    }

    // variables to manage timeout-------
    struct input_event our_magical_event;
    unsigned got_event = 0; // if true then we have got an event, otherwise we have timed out

    struct pollfd my_fd;
    my_fd.fd = fd;
    my_fd.events = POLLIN;
    // ----------------------------------

    rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
        goto out;
    }

    int err;
    int uifd;
    struct libevdev_uinput *uidev;

    uifd = open("/dev/uinput", O_RDWR);
    if (uifd < 0) {
        printf("uifd < 0 (Do you have the right privileges?)\n");
        return -errno;
    }

    err = libevdev_uinput_create_from_device(dev, uifd, &uidev);
    if (err != 0)
        return err;

    int grab = libevdev_grab(dev, LIBEVDEV_GRAB);
    if (grab < 0) {
        printf("grab < 0\n");
        return -errno;
    }

    do {
        int pending_events = libevdev_has_event_pending(dev);
        got_event = 0;
        struct timespec timeout;

        if (pending_events < 0) {
            perror("pending");
            exit(1);
        }

        if (pending_events) {
            /* printf("SOME EVENT PENDING\n"); */
            got_event = 1;

            // read next event begin ################################
            rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL|LIBEVDEV_READ_FLAG_BLOCKING, &our_magical_event);
            if (rc == LIBEVDEV_READ_STATUS_SYNC) {
                printf("janus_key: dropped\n");
                while (rc == LIBEVDEV_READ_STATUS_SYNC) {
                    rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &our_magical_event);
                }
                printf("janus_key: re-synced\n");
            }
        } else {
            /* printf("NO EVENT PENDING\n"); */
            // find that mod_key whose `delayed_down` is true and has the
            // soonest `send_down_at`, if any (there might not be a mod_key that
            // satisfies those conditions)
            int soonest_index = -1;
            struct timespec soonest_val = {.tv_sec = 0, .tv_nsec = 0};
            for (size_t i = 1; i < sizeof(mod_map)/sizeof(mod_map[0]); i++) {
                if (mod_map[i].delayed_down && timespec_cmp(&mod_map[i].send_down_at, &soonest_val) == 1) {
                    soonest_val = mod_map[i].send_down_at;
                    soonest_index = i;
                }
            }
            // calculate timeout
            clock_gettime(CLOCK_MONOTONIC, &now);

            /* printf("soonest_index: %d\n", soonest_index); */
            /* printf("soonest_val.tv_sec: %lu\n", soonest_val.tv_sec); */
            /* printf("soonest_val.tv_nsec: %lu\n", soonest_val.tv_nsec); */
            /* printf("now.tv_sec %lu\n", now.tv_sec); */
            /* printf("now.tv_nsec %lu\n", now.tv_nsec); */
            /* printf("mod_map[soonest_index].send_down_at.tv_sec %lu\n", mod_map[soonest_index].send_down_at.tv_sec); */
            /* printf("mod_map[soonest_index].send_down_at.tv_nsec %lu\n", mod_map[soonest_index].send_down_at.tv_nsec); */
            /* printf("compare: %d\n", timespec_cmp(&now, &(mod_map[soonest_index].send_down_at))); */
            timespec_subtract(&now, &(mod_map[soonest_index].send_down_at), &timeout);
            long timeout_ms = soonest_index == -1
                              ? 5000000 // default timeout (right?)
                              : timespec_to_ms(&timeout);
            /* printf("timeout in ms: %lu\n", timespec_to_ms(&timeout)); */
            /* printf("BEFORE BLOCKING\n"); */
            int cond = soonest_index == -1 || timespec_cmp(&now, &mod_map[soonest_index].send_down_at) == 1;
            if (cond && poll(&my_fd, 1, timeout_ms)) {
                /* printf("POLL RETURNED TRUE. WE HAVE AN EVENT.\n"); */
                // we didn't time out, so we can read an event
                //printf("WE DIDN'T TIME OUT, SO WE CAN READ AN EVENT\n");
                // read next event begin ################################
                rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL|LIBEVDEV_READ_FLAG_BLOCKING, &our_magical_event);
                if (rc == LIBEVDEV_READ_STATUS_SYNC) {
                    //printf("janus_key: dropped\n");
                    while (rc == LIBEVDEV_READ_STATUS_SYNC) {
                        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &our_magical_event);
                    }
                    //printf("janus_key: re-synced\n");
                }
            }
        }

        /* printf("GOING TO HANDLE TIMEOUTS\n"); */

        // handle timers
        for (size_t i = 0; i < sizeof(mod_map)/sizeof(mod_map[0]); i++) {
            //printf("HANDLING TIMERS\n");
            clock_gettime(CLOCK_MONOTONIC, &now);
            //printf("now: %lu\n", timespec_to_ms(&now));
            //printf("send_down_at: %lu\n", timespec_to_ms(&(mod_map[i].send_down_at)));

            // - we want to send the delayed down if
            //   - now is not less than send_down_at
            //   - delayed_down is true
            //   - now - send_down_at is <= 0
            /* printf("now.tv_sec: %lu\n", now.tv_sec); */
            /* printf("now.tv_nsec: %lu\n", now.tv_nsec); */
            /* printf("mod_map[i].send_down_at.tv_sec: %lu\n", mod_map[i].send_down_at.tv_sec); */
            /* printf("mod_map[i].send_down_at.tv_nsec: %lu\n", mod_map[i].send_down_at.tv_nsec); */

            timespec_subtract(&now, &(mod_map[i].send_down_at), &timeout);
            /* int ms = timespec_to_ms(&timeout); */
            /* printf("is timeout %lu passed?: %s\n", i, timespec_cmp(&now, &(mod_map[i].send_down_at)) != 1 ? "yes" : "no"); */
            /* printf("should timeout %lu be taken into consideration?: %s\n", i, mod_map[i].delayed_down == 1 ? "yes" : "no"); */
            /* printf("are ms less than or equal to 0?: %d\n", ms <= 0); */
            /* printf("ms: %d\n", ms); */

            int send_delay_down =  mod_map[i].delayed_down && timespec_cmp(&now, &(mod_map[i].send_down_at)) != 1 /*&& ms <= 0*/;
            if (send_delay_down) {
                /* printf("SENDING A DELAYED DOWN EVENT!!!\n"); */
                /* printf("i: %lu\n", i); */
                /* printf("mod_map[i].delayed_down: %d\n", mod_map[i].delayed_down); */
                if (mod_map[i].last_secondary_function_value_sent != 1) {
                    send_key_ev_and_sync(uidev, mod_map[i].secondary_function, 1);
                } else printf("refraining from sending %d %d\n", mod_map[i].secondary_function, 1);
                mod_map[i].last_secondary_function_value_sent = 1;
                //printf("SETTING RELEVANT MOD_KEY FLAG TO 0!!!\n");
                mod_map[i].delayed_down = 0;
            }
        }

        // handle new event if we have one
        if (got_event && rc == LIBEVDEV_READ_STATUS_SUCCESS && our_magical_event.type == EV_KEY) {
            /* printf("HANDLING %d, %d!\n", our_magical_event.code, our_magical_event.value); */
            handle_ev_key(uidev, our_magical_event.code, our_magical_event.value);
        }

    } while (rc == LIBEVDEV_READ_STATUS_SYNC || rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == -EAGAIN);

    if (rc != LIBEVDEV_READ_STATUS_SUCCESS && rc != -EAGAIN)
        fprintf(stderr, "Failed to handle events: %s\n", strerror(-rc));

    rc = 0;
out:
    libevdev_free(dev);

    return rc;
}
