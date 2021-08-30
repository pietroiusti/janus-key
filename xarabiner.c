// SPDX-License-Identifier: MIT
/*
 * Copyright © 2013 Red Hat, Inc.
 */

/*
  
  Translation of xtramodifier.py into c.

 */

// #include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "libevdev/libevdev.h"

#include "libevdev/libevdev-uinput.h"

#include <unistd.h>
#include <time.h>

// Config vars
unsigned int mod1 = KEY_CAPSLOCK;
unsigned int mod1_secondary_function = KEY_LEFTMETA;
unsigned int mod2 = KEY_ENTER;
unsigned int mod2_secondary_function = KEY_RIGHTMETA;
float max_delay = 0.3;
    
// Flags
int last_input_was_special_combination = 0;
int mod1_down_or_held = 0;
int mod2_down_or_held = 0;

// Variables for calculating delay
long int mod1_last_time_down = 0;
long int mod2_last_time_down = 0;

// Hold current time when needed
long int now;

static void
print_abs_bits(struct libevdev *dev, int axis)
{
    const struct input_absinfo *abs;

    if (!libevdev_has_event_code(dev, EV_ABS, axis))
	return;

    abs = libevdev_get_abs_info(dev, axis);

    printf("	Value	%6d\n", abs->value);
    printf("	Min	%6d\n", abs->minimum);
    printf("	Max	%6d\n", abs->maximum);
    if (abs->fuzz)
	printf("	Fuzz	%6d\n", abs->fuzz);
    if (abs->flat)
	printf("	Flat	%6d\n", abs->flat);
    if (abs->resolution)
	printf("	Resolution	%6d\n", abs->resolution);
}

static void
print_code_bits(struct libevdev *dev, unsigned int type, unsigned int max)
{
    unsigned int i;
    for (i = 0; i <= max; i++) {
	if (!libevdev_has_event_code(dev, type, i))
	    continue;

	printf("    Event code %i (%s)\n", i, libevdev_event_code_get_name(type, i));
	if (type == EV_ABS)
	    print_abs_bits(dev, i);
    }
}

static void
print_bits(struct libevdev *dev)
{
    unsigned int i;
    printf("Supported events:\n");

    for (i = 0; i <= EV_MAX; i++) {
	if (libevdev_has_event_type(dev, i))
	    printf("  Event type %d (%s)\n", i, libevdev_event_type_get_name(i));
	switch(i) {
	case EV_KEY:
	    print_code_bits(dev, EV_KEY, KEY_MAX);
	    break;
	case EV_REL:
	    print_code_bits(dev, EV_REL, REL_MAX);
	    break;
	case EV_ABS:
	    print_code_bits(dev, EV_ABS, ABS_MAX);
	    break;
	case EV_LED:
	    print_code_bits(dev, EV_LED, LED_MAX);
	    break;
	}
    }
}

static void
print_props(struct libevdev *dev)
{
    unsigned int i;
    printf("Properties:\n");

    for (i = 0; i <= INPUT_PROP_MAX; i++) {
	if (libevdev_has_property(dev, i))
	    printf("  Property type %d (%s)\n", i,
		   libevdev_property_get_name(i));
    }
}

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

int
main(int argc, char **argv)
{
    struct libevdev *dev = NULL;
    const char *file;
    int fd;
    int rc = 1;

    if (argc < 2)
	goto out;

    sleep(1);

    file = argv[1];
    fd = open(file, O_RDONLY);
    if (fd < 0) {
	perror("Failed to open device");
	goto out;
    }

    rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
	fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
	goto out;
    }

    // see
    // https://www.freedesktop.org/software/libevdev/doc/latest/group__uinput.html#details
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
	    //print_event(&ev);

	
	    if (ev.type == EV_KEY) { // if type is EV_KEY
		if (ev.code == mod1) {
		    if (ev.value == 1) {
			mod1_down_or_held = 1;
			printf("mod1_down_or_held = 1\n");
			last_input_was_special_combination = 0;
			printf("last_input_was_special_combination = 0\n");
			time(&mod1_last_time_down);
			printf("time(&mod1_last_time_down)\n");
		    } else if (ev.value == 2) {
			mod1_down_or_held = 1;
			printf("mod1_down_or_held = 1\n");
			last_input_was_special_combination = 0;
			printf("last_input_was_special_combination = 0\n");
		    } else {
			mod1_down_or_held = 0;
			printf("mod1_down_or_held = 0\n");
			time(&now);
			if (mod2_down_or_held) {
			    if (difftime(now, mod1_last_time_down) < max_delay) {
				last_input_was_special_combination = 1;
				printf("last_input_was_special_combination = 1\n");
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod2_secondary_function, 1);
				printf("mod2_secondary_function DOWN\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod1, 1);
				printf("mod1 DOWN\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod1, 0);
				printf("mod1 UP\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod2_secondary_function, 0);
				printf("mod2_secondary_function UP\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;				
			    } else { // Avoid ``locking'' mod1's secondary function down
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod1_secondary_function, 0);
				printf("mod1_secondary_function UP\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;
			    }
			} else {
			    if (last_input_was_special_combination) {
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod1_secondary_function, 0);
				printf("mod1_secondary_function UP\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;
			    } else {
				if (difftime(now, mod1_last_time_down) < max_delay) {
				    err = libevdev_uinput_write_event(uidev, EV_KEY, mod1, 1);
				    printf("mod1 DOWN\n");
				    if (err != 0)
					return err;
				    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				    if (err != 0)
					return err;
				    err = libevdev_uinput_write_event(uidev, EV_KEY, mod1, 0);
				    printf("mod1 UP\n");
				    if (err != 0)
					return err;
				    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				    if (err != 0)
					return err;
				} else { // Avoid ``locking'' mod1's secondary function down
				    err = libevdev_uinput_write_event(uidev, EV_KEY, mod1_secondary_function, 0);
				    printf("mod1_secondary_function UP\n");
				    if (err != 0)
					return err;
				    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				    if (err != 0)
					return err;
				}
			    }
			}
		    }
		} else if (ev.code == mod2) {
		    if (ev.value == 1) {
			mod2_down_or_held = 1;
			printf("mod2_down_or_held = 1\n");
			last_input_was_special_combination = 0;
			printf("last_input_was_special_combination = 0\n");
			time(&mod2_last_time_down);
			printf("time(&mod1_last_time_down)\n");
		    } else if (ev.value == 2) {
			mod2_down_or_held = 1;
			printf("mod2_down_or_held = 1\n");
			last_input_was_special_combination = 0;
			printf("last_input_was_special_combination = 0\n");
		    } else {
			mod2_down_or_held = 0;
			printf("mod2_down_or_held = 0\n");
			time(&now);
			if (mod1_down_or_held) {
			    if (difftime(now, mod2_last_time_down) < max_delay) {
				last_input_was_special_combination = 1;
				printf("last_input_was_special_combination = 1\n");
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod1_secondary_function, 1);
				printf("mod1_secondary_function DOWN\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod2, 1);
				printf("mod2 DOWN\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod2, 0);
				printf("mod2 UP\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod1_secondary_function, 0);
				printf("mod1_secondary_function UP\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;				
			    } else { // Avoid ``locking'' mod2's secondary function down
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod2_secondary_function, 0);
				printf("mod2_secondary_function UP\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;
			    }
			} else {
			    if (last_input_was_special_combination) {
				err = libevdev_uinput_write_event(uidev, EV_KEY, mod2_secondary_function, 0);
				printf("mod2_secondary_function UP\n");
				if (err != 0)
				    return err;
				err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				if (err != 0)
				    return err;
			    } else {
				if (difftime(now, mod2_last_time_down) < max_delay) {
				    err = libevdev_uinput_write_event(uidev, EV_KEY, mod2, 1);
				    printf("mod2 DOWN\n");
				    if (err != 0)
					return err;
				    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				    if (err != 0)
					return err;
				    err = libevdev_uinput_write_event(uidev, EV_KEY, mod2, 0);
				    printf("mod2 UP\n");
				    if (err != 0)
					return err;
				    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				    if (err != 0)
					return err;
				} else { // Avoid ``locking'' mod1's secondary function down
				    err = libevdev_uinput_write_event(uidev, EV_KEY, mod2_secondary_function, 0);
				    printf("mod2_secondary_function UP\n");
				    if (err != 0)
					return err;
				    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
				    if (err != 0)
					return err;
				}
			    }
			}
		    }
		} else {
		    if (ev.value == 1) {
			if (mod1_down_or_held) {
			    last_input_was_special_combination = 1;
			    printf("last_input_was_special_combination = 1\n");
			    err = libevdev_uinput_write_event(uidev, EV_KEY, mod1_secondary_function, 1);
			    printf("mod1_secondary_function DOWN\n");
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_KEY, ev.code, 1);
			    printf("%s DOWN\n", libevdev_event_code_get_name(ev.type, ev.code));
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
			    if (err != 0)
				return err;			    
			} else if (mod2_down_or_held) {
			    last_input_was_special_combination = 1;
			    printf("last_input_was_special_combination = 1\n");
			    err = libevdev_uinput_write_event(uidev, EV_KEY, mod2_secondary_function, 1); // necessary?
			    printf("mod2_secondary_function DOWN\n");
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_KEY, ev.code, 1);
			    printf("%s DOWN\n", libevdev_event_code_get_name(ev.type, ev.code));
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
			    if (err != 0)
				return err;
			} else {
			    last_input_was_special_combination = 0;
			    printf("last_input_was_special_combination = 0\n");
			    err = libevdev_uinput_write_event(uidev, EV_KEY, ev.code, 1);
			    printf("%s DOWN\n", libevdev_event_code_get_name(ev.type, ev.code));
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
			    if (err != 0)
				return err;
			}			
		    } else if (ev.value == 2) {
			if (mod1_down_or_held) {
			    last_input_was_special_combination = 1;
			    printf("last_input_was_special_combination = 1\n");
			    err = libevdev_uinput_write_event(uidev, EV_KEY, mod1_secondary_function, 2); // necessary?
			    printf("mod1_secondary_function HELD\n");
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_KEY, ev.code, 2);
			    printf("%s HELD\n", libevdev_event_code_get_name(ev.type, ev.code));
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
			    if (err != 0)
				return err;
			} else if (mod2_down_or_held) {
			    last_input_was_special_combination = 1;
			    printf("last_input_was_special_combination = 1\n");
			    err = libevdev_uinput_write_event(uidev, EV_KEY, mod2_secondary_function, 2); // necessary?
			    printf("mod2_secondary_function HELD\n");
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_KEY, ev.code, 2);
			    printf("key(?) HELD\n");
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
			    if (err != 0)
				return err;
			} else {
			    last_input_was_special_combination = 0;
			    printf("last_input_was_special_combination = 0\n");
			    err = libevdev_uinput_write_event(uidev, EV_KEY, ev.code, 2);
			    printf("%s HELD\n", libevdev_event_code_get_name(ev.type, ev.code));
			    if (err != 0)
				return err;
			    err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
			    if (err != 0)
				return err;
			}
		    } else {
			err = libevdev_uinput_write_event(uidev, EV_KEY, ev.code, 0);
			printf("%s UP\n", libevdev_event_code_get_name(ev.type, ev.code));
			if (err != 0)
			    return err;
			err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
			if (err != 0)
			    return err;
		    }
		}
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
