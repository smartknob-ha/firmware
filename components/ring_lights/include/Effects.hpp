#ifndef EFFECTS_HPP
#define EFFECTS_HPP

#include <led_strip.h>

#include "Declaration.hpp"

namespace ringLights {

    class effects {
    public:
        static void pointer(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg);
        static void percent(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg);
        static void fill(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg);
        static void gradient(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg);
        static void skip(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg);
        static void rainbowUniform(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg);
        static void rainbowRadial(rgb_t (&buffer)[NUM_LEDS], effectMsg& msg);

        static effectFunc get(RingLightEffect effect) { return m_mapper[effect]; };

    private:
        static const inline char                               TAG[] = "Ring light effects";
        static inline const etl::array<effectFunc, EFFECT_MAX> m_mapper{
                &effects::pointer,
                &effects::percent,
                &effects::fill,
                &effects::gradient,
                &effects::skip,
                &effects::rainbowUniform,
                &effects::rainbowRadial};
    };

}; // namespace ringLights

#endif // EFFECTS_HPP