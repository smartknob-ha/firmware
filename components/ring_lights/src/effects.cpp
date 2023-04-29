#include "../include/effects.hpp"
#include <math.h>

#include "esp_log.h"

namespace ring_lights {

//#define DEGREE_TO_RADIAN_FACTOR M_PI / 180
const double DEGREE_TO_RADIAN_FACTOR = M_PI / 180;

void effects::pointer(uint8_t (&buffer)[RING_LIGHTS_BUFFER_SIZE], effect_msg& msg) {
    uint16_t angle_degrees = msg.param_a % 360;
    double angle_radian = angle_degrees * DEGREE_TO_RADIAN_FACTOR;
    uint8_t led_to_turn_on = (angle_radian / M_PI) * NUM_LEDS / 2;
    for (uint8_t i = 0; i < led_to_turn_on; i ++) {
        set_led_color(buffer, i, {0, 0, 0});
    }
    set_led_color(buffer, led_to_turn_on, hsv2rgb_raw(msg.primary_color));
    for (uint8_t i = led_to_turn_on+1; i < NUM_LEDS; i ++) {
        set_led_color(buffer, i, {0, 0, 0});
    }
}

void effects::rainbow_uniform(uint8_t (&buffer)[RING_LIGHTS_BUFFER_SIZE], effect_msg& msg) {
    static uint8_t i = 0;
    i ++;
    hsv_t color {
        .hue = i,
        .saturation = 255,
        .value = 255
    };

    rgb_t rgb = hsv2rgb_rainbow(color);

    // Give all LED's the same colour
    for (int i = 0; i < NUM_LEDS; i ++) {
        set_led_color(buffer, i, rgb);
    }
}

void effects::set_led_color(uint8_t (&buffer)[RING_LIGHTS_BUFFER_SIZE], uint8_t index, rgb_t color) {
    uint8_t l_index = index * RING_LIGHTS_BYTES_PER_LED;
    buffer[l_index] = color.r;
    buffer[l_index+1] = color.g;
    buffer[l_index+2] = color.b;
}

}; /* namespace ring_lights */