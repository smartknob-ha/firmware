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
	/*
	* Approximate a point on an angle
	*
	* param_a: [int] angle center of point
	* param_b: [int] angle of width of point
	*
	* primary_color: base color, fades at from center
	*/
	POINTER,

	/*
	* Fills a bar around the smartknob from start to percentage
	*
	* param_a: [int]  starting angle, between 180 and 359
	* param_b: [int]  ending angle, between 0 and 179
	* param_c: [int]  percentage from 0 to 100
	*
	* primary_color: base color
	* secondary_color: gradient color if provided (any non-zero)
	*/
	PERCENT,

	/*
	* Fills all LED's with the same color
	*
	* primary_color: base color
	*/
	FILL,

	/*  -- NOT IMPLEMENTED --
	* Static gradient
	*
	* param_a: [int]  gradient angle, between 0 and 359
	* param_b: [int]  gradient width, between 0 and 100 (percent)
	* param_c: [int]  gradient center, between 0 and 100 (percent) (0 corresponding to the bottom and 100 to the top)
	*
	* primary_color: base color
	* secondary color: gradient color
	*/
	GRADIENT,

	/*	-- NOT IMPLEMENTED --
	* Set every other led to another color
	*
	* primary_color: color for led's on even index
	* secondary_color: color for led's on odd index
	*/
	SKIP,

	/*
	* Cycles all LED's through a rainbow at the same time
	*/
	RAINBOW_UNIFORM,

	/* -- NOT IMPLEMENTED --
	* Moves a rainbow around the ring
	*
	* param_a: [int] rotation per tick
	*/
	RAINBOW_RADIAL,

	EFFECT_MAX  // Leave this at the bottom
};

struct effect_msg {
	ring_light_effect effect = RAINBOW_UNIFORM;
	int16_t param_a = 0, param_b = 0, param_c = 0;
	hsv_t primary_color{0, 0, 0}, secondary_color{0, 0, 0};
};

typedef void (* effect_func)(rgb_t (&)[NUM_LEDS], effect_msg&);

}; /* namespace ring_lights */

#endif /* RING_LIGHTS_DECLARATIONS_HPP */