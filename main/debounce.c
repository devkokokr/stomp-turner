#include "debounce.h"

void debounce_init(debounced_input_t *d, gpio_num_t pin)
{
    d->pin = pin;
    d->stable_level = d->candidate_level = gpio_get_level(pin);
    d->candidate_count = 0;
}

bool debounce_update(debounced_input_t *d)
{
    int level = gpio_get_level(d->pin);

    if (level == d->candidate_level) {
        if (d->candidate_count < DEBOUNCE_STABLE_POLLS) {
            d->candidate_count++;
        }
    } else {
        d->candidate_level = level;
        d->candidate_count = 1;
    }

    if (d->candidate_count >= DEBOUNCE_STABLE_POLLS && d->stable_level != d->candidate_level) {
        d->stable_level = d->candidate_level;
        return true;
    }
    return false;
}
