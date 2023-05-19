#include "../include/effects.hpp"
#include <math.h>
#include <cmath>

#include "esp_log.h"

namespace ring_lights {

#define DEGREE_PER_LED 360 / NUM_LEDS

// Reset an angle to between 0 and +inf
#define WRAP_NEGATIVE_ANGLE(a) (a < 0 ? 360 - a : a)

// Just make sure to enter your angles in clockwise order
int_fast16_t GET_CLOCKWISE_DIFF(int_fast16_t a, int_fast16_t b) {
	int_fast16_t diff = b - a;
	if (diff < 0) {
		return diff + 360;
	}
	return diff;
}

int_fast16_t GET_COUNTER_CLOCKWISE_DIFF(int_fast16_t a, int_fast16_t b) {
	int_fast16_t diff = a - b;
	if (diff < 0) {
		return diff + 360;
	}
	return diff;
}

bool IS_BETWEEN_A_B_CLOCKWISE(int_fast16_t a, int_fast16_t b, int_fast16_t c) {
	int_fast16_t relative_total_width = a + GET_CLOCKWISE_DIFF(a, b);
	int_fast16_t relative_angle = a + GET_CLOCKWISE_DIFF(a, c);

	return (relative_angle <= relative_total_width);
}

template<typename T>
double GET_LED_ANGLE(T led) {
	static_assert(std::is_arithmetic<T>::value);
	double angle = 270 - (led * DEGREE_PER_LED);
	if (angle < 0) {
		angle = 360 + angle;
	}
	return angle;
}

void effects::pointer(rgb_t (&buffer)[NUM_LEDS], effect_msg& msg) {
	int_fast16_t angle_degrees = WRAP_NEGATIVE_ANGLE(msg.param_a);
	int_fast16_t width_degree = msg.param_b;
	int_fast16_t width_half_pointer_degree = width_degree / 2;

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

void effects::percent(rgb_t (&buffer)[NUM_LEDS], effect_msg& msg) {
	int_fast16_t start = msg.param_a;
	int_fast16_t end = msg.param_b;

	// Scale `end` to percentage
	double percent = (double)msg.param_c / 100;
	int_fast16_t total_width = GET_CLOCKWISE_DIFF(start, end);
	int_fast16_t correct_end = start + (total_width * (percent));
	correct_end %= 360;

	for (int_fast8_t i = 0; i < NUM_LEDS; i ++) {
		double current_degree = GET_LED_ANGLE(i);
		if (IS_BETWEEN_A_B_CLOCKWISE(start, correct_end, current_degree)) {
			buffer[i] = hsv2rgb_rainbow(msg.primary_color);
		} else {
			buffer[i] = {0, 0, 0};
		}
	}
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