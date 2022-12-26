#include "./janus-key.h"
#include <linux/input-event-codes.h>

/* janus_key janus_map[] = { */
/*     // key                1st function     2nd function */
/*     {  KEY_CAPSLOCK,      KEY_ESC,         KEY_LEFTALT     }, */
/*     {  KEY_ENTER,         0,               KEY_RIGHTALT    }, */
/*     {  KEY_RIGHTALT,      KEY_RIGHTCTRL }, */
/*     {  KEY_LEFTALT,       KEY_LEFTCTRL  }, */
/*     {  KEY_RIGHTMETA,     KEY_RIGHTALT  }, */
/*     {  KEY_LEFTMETA,      KEY_LEFTALT   }, */
/*     {  KEY_COMPOSE,       KEY_RIGHTMETA }, */
/*     {  KEY_LEFTCTRL,      KEY_LEFTMETA  }, */
/*     {  KEY_ESC,           KEY_CAPSLOCK  } */
/* }; */

mod_key mod_map[] = {
    // key                1st function     2nd function
    {  KEY_CAPSLOCK,      KEY_ESC,         KEY_LEFTALT     },
    {  KEY_ENTER,         0,               KEY_RIGHTALT    },
    {  KEY_RIGHTALT,      KEY_RIGHTCTRL },
    {  KEY_LEFTALT,       KEY_LEFTCTRL  },
    {  KEY_RIGHTCTRL,     KEY_RIGHTALT  },
    {  KEY_LEFTCTRL,      KEY_LEFTALT   },
    //{  KEY_LEFTMETA,      KEY_LEFTMETA  },
    {  KEY_SYSRQ,         KEY_RIGHTMETA },
    {  KEY_ESC,           KEY_CAPSLOCK  }
};

combo combo_map[] = {
    {  KEY_CAPSLOCK,      KEY_ESC,         { KEY_ESC, 0 } }
};

// Delay in milliseconds. 
unsigned int max_delay = 300; // If a key is held down for a time
			      // greater than max_delay, then, when
			      // released, it will not send its
			      // primary function
