#include "../include/effects.hpp"
#include <math.h>
#include <cmath>

#include "esp_log.h"

namespace ring_lights {

#define DEGREE_PER_LED 360 / NUM_LEDS

void effects::pointer(rgb_t (&buffer)[NUM_LEDS], effect_msg& msg) {
	// Convert all angles to radians, rotate so 0 is at the top of the smartknob
	// and account that we don't have an LED in the middle
	volatile int_fast16_t angle_degrees = (msg.param_a + 270 + (DEGREE_PER_LED / 2)) % 360;
	volatile int_fast16_t width_degree = msg.param_b;
	volatile int_fast16_t width_half_pointer_degree = width_degree / 2;

	hsv_t color = msg.primary_color;

	// Start with an empty buffer
	for (int_fast8_t i = NUM_LEDS - 1; i >= 0; i --) {
		double current_degree = i * DEGREE_PER_LED;
		double degrees_to_center = 180 - abs(abs(angle_degrees - current_degree) - 180);
		double progress = 0.0;

		if (degrees_to_center <= width_half_pointer_degree) {
			progress = degrees_to_center / width_degree;
		}

		if (-1 <= progress && progress <= 0) {
			color.value = msg.primary_color.value * abs(progress);
		} else if (0 < progress && progress <= 1) {
			color.value = msg.primary_color.value * (1 - progress);
		}

		buffer[wrap_index(i)] = hsv2rgb_rainbow(color);
		color.value = 0;
	}
}

void effects::pointer(uint8_t (&buffer)[RING_LIGHTS_BUFFER_SIZE], effect_msg& msg) {
    uint16_t angle_degrees = msg.param_a % 360;
    double angle_radian = angle_degrees * DEGREE_TO_RADIAN_FACTOR;
    uint8_t led_to_turn_on = (angle_radian / M_PI) * NUM_LEDS / 2;
    for (uint8_t i = 0; i < led_to_turn_on; i ++) {
        set_led_color(buffer, i, {0, 0, 0});
    }

void effects::fill(rgb_t (&buffer)[NUM_LEDS], effect_msg& msg) {
	for (int_fast8_t i = 0; i < NUM_LEDS; i++) {
		buffer[i] = hsv2rgb_rainbow(msg.primary_color);
    }
}

void effects::rainbow_uniform(rgb_t (&buffer)[NUM_LEDS], effect_msg& msg) {
    static uint8_t i = 0;
    i ++;
    hsv_t color {
        .hue = i,
		.saturation = msg.primary_color.saturation,
		.value = msg.primary_color.value
    };

    rgb_t rgb = hsv2rgb_rainbow(color);

    // Give all LED's the same colour
    for (int i = 0; i < NUM_LEDS; i ++) {
		buffer[i] = rgb;
    }
}

template<typename T>
T effects::wrap_index(T index) {
	static_assert(std::is_arithmetic<T>::value);

	if (0 <= index && index < NUM_LEDS) {
		return index;
	}

	return std::abs(index % NUM_LEDS);
}

}; /* namespace ring_lights */