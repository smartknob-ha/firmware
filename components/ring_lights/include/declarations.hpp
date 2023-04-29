#ifndef RING_LIGHTS_DECLARATIONS_HPP
#define RING_LIGHTS_DECLARATIONS_HPP

#include "freertos/FreeRTOS.h"

#include <led_strip.h>
#include <etl/array.h>

namespace ring_lights {

#define NUM_LEDS CONFIG_LED_STRIP_NUM
#define DATA_PIN CONFIG_LED_STRIP_GPIO
#define CLOCK_PIN -1

#define RING_LIGHTS_BYTES_PER_LED 3
#define RING_LIGHTS_BUFFER_SIZE NUM_LEDS * RING_LIGHTS_BYTES_PER_LED
#define EFFECT_TRANSITION_TICKS 1000 / portTICK_PERIOD_MS

// This way you could set some basic colors, but no patterns
enum ring_light_effect {
    // Approximate a point on an angle param_a
    // Uses primary_color for point color
    POINTER,
    // Fill circle starting from param_a until param_a + param_b
    // Base primary color, gradients into secondary color if provided
    PERCENT,
    // Fill entire circle with primary color
    FILL,
    // Gradient from primary_color into secondary_color
    // using param_a as starting angle, param_b as direction angle
    GRADIENT,
    // Set every even LED to primary color, every odd LED to secondary color
    SKIP,
    // No parameters used, cycles all LED's through a rainbow at the same time
    RAINBOW_UNIFORM,
    // Moves a rainbow colour patern around the ring
    // using param_a as angle rotated per tick
    RAINBOW_RADIAL,

    EFFECT_MAX  // Leave this at the bottom
};

struct effect_msg {
    ring_light_effect effect = RAINBOW_UNIFORM;
    uint16_t param_a = 0, param_b = 0;
    hsv_t primary_color{0, 0, 0}, secondary_color{0, 0, 0};
};

typedef void (*effect_func)(uint8_t (&)[RING_LIGHTS_BUFFER_SIZE], effect_msg&);

}; /* namespace ring_lights */

#endif /* RING_LIGHTS_DECLARATIONS_HPP */