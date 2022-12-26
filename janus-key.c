/*
  
    janus-key. Give keys a double function.

    Copyright (C) 2021, 2022  Giulio Pietroiusti

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
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

#include "libevdev/libevdev.h"
#include "libevdev/libevdev-uinput.h"

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
// Return -1 if *tp1 < *tp2, 0 if *tp1 == *tp2, 1 if *tp1 < *tp2
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
    c->tv_sec = a->tv_sec + b->tv_sec;
    c->tv_nsec = a->tv_nsec + b->tv_nsec;
    if (c->tv_nsec >= 1000000000) { // overflow
        c->tv_nsec -= 1000000000;
        c->tv_sec++;
    }
}

// Post the EV_KEY event of code `code` and `value` through the
// uninput device `*uidev` and send a (EV_SYN, SYN_REPORT, 0) event
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
    
    //printf("Sending %u %u\n", code, value);
}

// For each janus key down or held send an EV_KEY event with its
// secondary function code and value `value`.
static void send_down_or_held_jks_secondary_function(const struct libevdev_uinput *uidev, int value) {
    for (int j = 0; j < sizeof(mod_map)/sizeof(mod_map[0]); j++) {
        if (mod_map[j].state == 1 || mod_map[j].state == 2 && mod_map[j].secondary_function > 0)  {
            send_key_ev_and_sync(uidev, mod_map[j].secondary_function, value);
        }
    }
}

// Post throught uidev, with value `value, the primary function
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
    int i = is_janus(code);
    if (i >= 0) {
        //printf("is_janus\n");
        if (value == 1) {
            mod_map[i].state = 1;
            clock_gettime(CLOCK_MONOTONIC, &mod_map[i].last_time_down);
            last_input_was_special_combination = 0;
        } else if (value == 2) {
            mod_map[i].state = 1;
            last_input_was_special_combination = 0;
        } else {
            mod_map[i].state = 0;
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (some_jk_are_down_or_held() >= 0) {
                timespec_add(&mod_map[i].last_time_down, &tp_max_delay, &tp_sum);
                if (timespec_cmp(&now, &tp_sum) == 1) { // if now - mod_map[i].last_time_down < max_delay
                    if (last_input_was_special_combination) {
                        send_key_ev_and_sync(uidev, mod_map[i].secondary_function, 0);
                    } else {
                        printf("foo\n"); //ex: Escape + Enter
                        last_input_was_special_combination = 1;
                        send_down_or_held_jks_secondary_function(uidev, 1); // refactor?
                        send_primary_function(uidev, mod_map[i].key, 1);
                        send_primary_function(uidev, mod_map[i].key, 0);
                        send_down_or_held_jks_secondary_function(uidev, 0);
                    }
                } else {
                    send_key_ev_and_sync(uidev, mod_map[i].secondary_function, 0);
                }
            } else {
                timespec_add(&mod_map[i].last_time_down, &tp_max_delay, &tp_sum);
                if (timespec_cmp(&now, &tp_sum) == 1) { // if now - mod_map[i].last_time_down < max_delay
                    if (last_input_was_special_combination) {
                        send_key_ev_and_sync(uidev, mod_map[i].secondary_function, 0);
                    } else {
                        send_primary_function(uidev, mod_map[i].key, 1);
                        send_primary_function(uidev, mod_map[i].key, 0);
                        last_input_was_special_combination = 0;
                    }
                } else {
                    send_key_ev_and_sync(uidev, mod_map[i].secondary_function, 0);
                }
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
                send_primary_function(uidev, code, 1);
            } else {
                last_input_was_special_combination = 0;
                send_primary_function(uidev, code, 1);
            }
        } else { // if (value == 0)
            // if code is in combo... TODO

            send_primary_function(uidev, code, 0);
        }
    }
}

/*
********************************************************************************
BEGIN functions from libevdev-1.11.0/tools/libevdev-events.c
********************************************************************************
SPDX-License-Identifier: MIT
Copyright © 2013 Red Hat, Inc.
*/

static int
print_event(struct input_event *ev)
{
    if (ev->type == EV_SYN)
        printf("Event: time %ld.%06ld, ++++++++++++++++++++ %s +++++++++++++++\n",
               ev->input_event_sec,
               ev->input_event_usec,
               libevdev_event_type_get_name(ev->type));
    else
        printf("Event: time %ld.%06ld, type %d (%s), code %d (%s), value %d\n",
               ev->input_event_sec,
               ev->input_event_usec,
               ev->type,
               libevdev_event_type_get_name(ev->type),
               ev->code,
               libevdev_event_code_get_name(ev->type, ev->code),
               ev->value);
    return 0;
}

static int
print_sync_event(struct input_event *ev)
{
    printf("SYNC: ");
    print_event(ev);
    return 0;
}

/* 
********************************************************************************
END functions from libevdev-1.11.0/tools/libevdev-events.c
********************************************************************************
*/

int
main(int argc, char **argv)
{
    tp_max_delay.tv_sec = 0;
    tp_max_delay.tv_nsec = max_delay * 1000000;

    struct libevdev *dev = NULL;
    const char *file;
    int fd;
    int rc = 1;

    if (argc < 2)
        goto out;

    usleep(100000); // let (KEY_ENTER), value 0 go through before
                    // grabbing the device

    file = argv[1];
    fd = open(file, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device\n");
        goto out;
    }

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
        struct input_event ev;
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL|LIBEVDEV_READ_FLAG_BLOCKING, &ev);
        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            printf("::::::::::::::::::::: dropped ::::::::::::::::::::::\n");
            while (rc == LIBEVDEV_READ_STATUS_SYNC) {
                print_sync_event(&ev);
                rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
            }
            printf("::::::::::::::::::::: re-synced ::::::::::::::::::::::\n");
        } else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            //printf("received: \n");
            //print_event(&ev);
            if (ev.type == EV_KEY) {
                handle_ev_key(uidev, ev.code, ev.value);
            }
        }
    } while (rc == LIBEVDEV_READ_STATUS_SYNC || rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == -EAGAIN);
    
    if (rc != LIBEVDEV_READ_STATUS_SUCCESS && rc != -EAGAIN)
        fprintf(stderr, "Failed to handle events: %s\n", strerror(-rc));
    
    rc = 0;
out:
    libevdev_free(dev);
    
    return rc;
}
