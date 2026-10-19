#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Configure the uptime timer and non-blocking onboard LED blink.
void timer_setup(void);

// Return the uptime counter atomically.
uint32_t timer_get_elapsed_seconds(void);

#endif