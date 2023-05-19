#ifndef EFFECTS_HPP
#define EFFECTS_HPP

#include <led_strip.h>

#include "declarations.hpp"

namespace ring_lights {

	class effects {
		public:
			static void pointer(rgb_t (&buffer)[NUM_LEDS], effect_msg& msg);
			static void percent(rgb_t (&buffer)[NUM_LEDS], effect_msg& msg);
			static void fill(rgb_t (&buffer)[NUM_LEDS], effect_msg& msg);
			static void gradient(rgb_t (&buffer)[NUM_LEDS], effect_msg& msg) {};
			static void skip(rgb_t (&buffer)[NUM_LEDS], effect_msg& msg) {};
			static void rainbow_uniform(rgb_t (&buffer)[NUM_LEDS], effect_msg& msg);

			static effect_func get(ring_light_effect effect) { return m_mapper[effect]; };
		private:
			static const inline char TAG[] = "Ring light effects";
			static inline const etl::array<effect_func, EFFECT_MAX> m_mapper {
				&effects::pointer,
				&effects::percent,
				&effects::fill,
				&effects::gradient,
				&effects::skip,
				&effects::rainbow_uniform
			};

			template<typename T>
			static T wrap_index(T index);
	};

}; /* namespace ring_lights */

#endif // EFFECTS_HPP