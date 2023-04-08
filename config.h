#include "./janus-key.h"

mod_key mod_map[] = {
    // key                1st function     2nd function
    {  KEY_CAPSLOCK,      KEY_ESC,         KEY_LEFTALT     }, // Change both CAPS' primary and secondary function
    {  KEY_ENTER,         0,               KEY_RIGHTALT    }, // Do not change ENTER's primary function
    {  KEY_ESC,           KEY_CAPSLOCK  }  // Only change ESC's primary function
};

// Delay in milliseconds. 
unsigned int max_delay = 300; // If a key is held down for a time
			      // greater than max_delay, then, when
			      // released, it will not send its
			      // primary function
