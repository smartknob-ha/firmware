#include "../include/effects.hpp"

#include <math.h>
#include <cmath>
#include "esp_log.h"

namespace ring_lights {

#define M_TAU (2 * M_PI)

#define DEGREE_PER_LED (360.0 / NUM_LEDS)
#define RAD_PER_LED (M_TAU / NUM_LEDS)

// Reset an angle to between 0 and +inf
#define WRAP_NEGATIVE_DEGREE(a) (a < 0 ? 360 - a : a)

#define GET_SMALLEST_DEGREE_DIFFERENCE(a, b) (180.0f - fabs(abs(a - b) - 180.0f))

#define DEGREE_TO_RADIAN(a) (a * M_PI / 180)

// Just make sure to enter your angles in clockwise order
int_fast16_t GET_CLOCKWISE_DIFF_DEGREES(int_fast16_t a, int_fast16_t b) {
	int_fast16_t diff = b - a;
	if (diff < 0) {
		return diff + 360;
	}
	return diff;
}

/* Calculates whether angle c is between angle a and b
*		in a clockwise direction
*  @returns how far the current angle is between a and b, clockwise.
*/
int_fast16_t IS_BETWEEN_A_B_CLOCKWISE_DEGREES(int_fast16_t a, int_fast16_t b, int_fast16_t c) {
	int_fast16_t relative_total_width = a + GET_CLOCKWISE_DIFF_DEGREES(a, b);
	int_fast16_t relative_angle = a + GET_CLOCKWISE_DIFF_DEGREES(a, c);

	return relative_angle - relative_total_width;
}

template<typename T>
double GET_LED_ANGLE_DEGREES(T led) {
	static_assert(std::is_arithmetic<T>::value);
	double angle = static_cast<double>(led) * static_cast<double>(DEGREE_PER_LED);
	if (angle < 0.0) {
		angle = 360.0 + angle;
	}
	return angle;
}

template<typename T>
double GET_LED_ANGLE_RAD(T led) {
	static_assert(std::is_arithmetic<T>::value);
    // (M_PI / 2) because radians start on the right (90 degrees)
	double angle = (led * RAD_PER_LED) + (M_PI / 2);
	if (angle < 0) {
		angle = M_TAU + angle;
	}
	if (angle > M_PI) {
		angle = angle - M_TAU;
	}
	return angle;
}

bool HSV_IS_EQUAL(hsv_t a, hsv_t b) {
	return a.h == b.h && a.s == b.s && a.v == b.v;
}

void effects::pointer(rgb_t (& buffer)[NUM_LEDS], effect_msg& msg) {
	auto angle_degrees = static_cast<float>(WRAP_NEGATIVE_DEGREE(msg.param_a) % 360);
	auto width_degree = static_cast<float>(msg.param_b);
	auto width_half_pointer_degree = static_cast<float>(width_degree) / 2.0f;

	hsv_t color = msg.primary_color;

	for (int_fast8_t i = 0; i < NUM_LEDS; i++) {
		float current_degree = static_cast<float>(i) * static_cast<float>(DEGREE_PER_LED);
		float degrees_to_center = GET_SMALLEST_DEGREE_DIFFERENCE(angle_degrees, current_degree);
		float progress = 0.0f;

		if (degrees_to_center <= width_half_pointer_degree) {
			progress = (width_half_pointer_degree - degrees_to_center) / width_half_pointer_degree;
		}

		if (0.0f <= progress && progress <= 1.0f) {
			color.value = static_cast<uint8_t>(static_cast<double>(msg.primary_color.value) * abs(progress));
		} /*else if (1.0f <= progress && progress <= 2.0f) {
			color.value = static_cast<uint8_t>(static_cast<double>(msg.primary_color.value) * (1 - (progress - 1)));
		}*/ else {
			color.value = 0;
		}

		buffer[i % NUM_LEDS] = hsv2rgb_rainbow(color);
		color.value = 0;
	}
}

void effects::percent(rgb_t (& buffer)[NUM_LEDS], effect_msg& msg) {
	int_fast16_t start = msg.param_a;
	int_fast16_t end = msg.param_b;

	// Scale `end` to percentage
	double percent = static_cast<double>(msg.param_c) / 100;
	int_fast16_t total_width = GET_CLOCKWISE_DIFF_DEGREES(start, end);
	int_fast16_t correct_end = start + (total_width * (percent));
	correct_end %= 360;

	for (int_fast8_t i = 0; i < NUM_LEDS; i++) {
		double current_degree = GET_LED_ANGLE_DEGREES(i);
		if (IS_BETWEEN_A_B_CLOCKWISE_DEGREES(start, correct_end, current_degree)) {
			buffer[i] = hsv2rgb_rainbow(msg.primary_color);
		} else {
			buffer[i] = {{0}, {0}, {0}};
		}
	}
}

void effects::fill(rgb_t (& buffer)[NUM_LEDS], effect_msg& msg) {
	for (auto& led : buffer) {
		led = hsv2rgb_rainbow(msg.primary_color);
	}
}

void effects::gradient(rgb_t (& buffer)[NUM_LEDS], effect_msg& msg) {
	// Since every opposite LED gets the same color,
	// the gradient buffer only needs to be half the size
	// of the LED buffer.
	const static uint_fast8_t GRADIENT_RESOLUTION = NUM_LEDS * 2;

	static hsv_t p_primary_color, p_secondary_color;
	static hsv_t GRADIENT_BUFFER[GRADIENT_RESOLUTION];

	// Only recalculate the gradient if the colors changed
	if (!HSV_IS_EQUAL(msg.primary_color, p_primary_color) || !HSV_IS_EQUAL(msg.secondary_color, p_secondary_color)) {
		hsv_fill_gradient2_hsv(GRADIENT_BUFFER, GRADIENT_RESOLUTION, msg.secondary_color, msg.primary_color,
		                       COLOR_SHORTEST_HUES);
		p_primary_color = msg.primary_color;
		p_secondary_color = msg.secondary_color;
	}

	double gradient_angle = msg.param_a % 360;

	// Divide by 50 so 100 percent covers the whole unit circle height
	double gradient_width = static_cast<double>(msg.param_b) / 50.0;
	// Subtract 1 so 50 percent will be 0, which is the center y of the unit circle
	double gradient_center = (static_cast<double>(msg.param_c) / 50.0) - 1;

	// Center is along the middle line, so upper and lower describe the start and
	// ending for both the left and the right side of the gradient on the circle
	double upper = gradient_center + (gradient_width / 2);
	double lower = gradient_center - (gradient_width / 2);

	for (int_fast16_t i = 0; i < NUM_LEDS; i++) {
		double current_degree = GET_LED_ANGLE_RAD(i);
		current_degree -= DEGREE_TO_RADIAN(gradient_angle);
		double y_pos = sin(current_degree);

		if (y_pos <= lower) {
			buffer[i] = hsv2rgb_rainbow(msg.secondary_color);
		} else if (y_pos >= upper) {
			buffer[i] = hsv2rgb_rainbow(msg.primary_color);
		} else {
            double progress = (y_pos - lower) / gradient_width;
			buffer[i] = hsv2rgb_rainbow(GRADIENT_BUFFER[static_cast<int>(std::round(progress * (GRADIENT_RESOLUTION - 1)))]);
		}
	}
}

void effects::skip(rgb_t (& buffer)[NUM_LEDS], ring_lights::effect_msg& msg) {
	for (int_fast16_t i = 0; i < NUM_LEDS; i ++) {
		if (i % 2) {
			buffer[i] = hsv2rgb_rainbow(msg.primary_color);
		} else {
			buffer[i] = hsv2rgb_rainbow(msg.secondary_color);
		}
	}
}

void effects::rainbow_uniform(rgb_t (& buffer)[NUM_LEDS], effect_msg& msg) {
	static uint8_t hsv_step = 0;
	hsv_step++;
	hsv_t color{
		.hue = hsv_step, .saturation = msg.primary_color.saturation, .value = msg.primary_color.value
	};

	rgb_t rgb = hsv2rgb_rainbow(color);

	// Give all LED's the same colour
	for (auto& led : buffer) {
		led = rgb;
	}
}

void effects::rainbow_radial(rgb_t (& buffer)[NUM_LEDS], effect_msg& msg) {
	static rgb_t rainbow_buffer[NUM_LEDS];
	static bool ran = false;
	if (!ran) {
		float scalar = static_cast<float>(UINT8_MAX) / (static_cast<float>(NUM_LEDS) - 1);
		for (uint8_t i = 0; i < NUM_LEDS; i ++) {
			float hue = std::fmin(static_cast<float>(i) * scalar, 255.0f);
			rainbow_buffer[i] = hsv2rgb_rainbow({.h = static_cast<uint8_t>(roundf(hue)), .s = 255, .v = 255});
			ran = true;
		}
	}

	static int_fast16_t p_angle = 0;
	p_angle = (p_angle + msg.param_a ) % 360;
	p_angle = p_angle <= 0 ? p_angle + 360 : p_angle;

	auto led_offset = static_cast<int_fast16_t>(p_angle / DEGREE_PER_LED) % NUM_LEDS;

	for (int_fast16_t i = 0; i < NUM_LEDS; i ++) {
		buffer[i] = rainbow_buffer[(i + led_offset) % NUM_LEDS];
	}
}

} /* namespace ring_lights */