#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"

// Number of consecutive polls a level must hold before it's treated as a
// real, debounced transition rather than switch/contact bounce.
#define DEBOUNCE_STABLE_POLLS 3

typedef struct {
    gpio_num_t pin;
    int stable_level;
    int candidate_level;
    uint8_t candidate_count;
} debounced_input_t;

// Latches the pin's current level as the debounced baseline (not treated as
// an edge). Call once before the first debounce_update().
void debounce_init(debounced_input_t *d, gpio_num_t pin);

// Returns true the moment `d` settles on a new stable level (a debounced
// edge). Call once per poll from a task loop.
bool debounce_update(debounced_input_t *d);
