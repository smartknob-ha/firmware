#ifndef LED_STRIP_HPP
#define LED_STRIP_HPP

#include <led_strip.h>

#include "component.hpp"
#include "declarations.hpp"
#include "effects.hpp"

namespace ring_lights {

#if defined(CONFIG_SK6812)
	#define LED_TYPE led_strip_type_t::LED_STRIP_SK6812
#elif defined(CONFIG_WS2812)
	#define LED_TYPE led_strip_type_t::LED_STRIP_WS2812
#elif defined(CONFIG_APA106)
	#define LED_TYPE led_strip_type_t::LED_STRIP_APA106
#elif defined(SM16703)
	#define LED_TYPE led_strip_type_t::LED_STRIP_SM16703
#else
	#error Please define a valid LED type using menuconfig
#endif

class ring_lights : public component, public has_queue<1, effect_msg, 0> {
public:
	ring_lights() {};
	~ring_lights() = default;

	/* Component override functions */
	virtual etl::string<50> get_tag() override { return TAG; };
	virtual res get_status() override;
	virtual res initialize() override;
	virtual res run() override;
	virtual res stop() override;

private:
	static const inline char TAG[] = "Ring lights";
	rgb_t m_current_effect_buffer[NUM_LEDS];
	rgb_t m_new_effect_buffer[NUM_LEDS];

	COMPONENT_STATUS m_status = UNINITIALIZED;
	bool m_run = false;
	uint32_t m_effect_transition_ticks_left = 0;

	effect_msg m_current_effect{
		.effect = RAINBOW_UNIFORM,
		.param_a = 0,
		.param_b = 0,
		.primary_color = {0, 0, 0},
		.secondary_color = {0, 0, 0}
	};

	effect_msg m_new_effect;

	static void start_flush(void*);
	void flush_thread();
	void transition_effect();

	effect_func m_current_effect_func_p = &effects::rainbow_uniform;
	effect_func m_new_effect_func_p = &effects::rainbow_uniform;

	inline static led_strip_t m_strip = {
		.type = LED_TYPE,
		.is_rgbw = false,
		.brightness = 192,
		.length = NUM_LEDS,
		.gpio = gpio_num_t(DATA_PIN),
		.channel = (rmt_channel_t) CONFIG_LED_RMT_CHANNEL,
		.buf = nullptr
	};
};

} /* namespace ring_lights */


#endif /* LED_STRIP_HPP */