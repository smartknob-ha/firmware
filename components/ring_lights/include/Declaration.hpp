#ifndef RING_LIGHTS_DECLARATIONS_HPP
#define RING_LIGHTS_DECLARATIONS_HPP

#include <etl/array.h>
#include <led_strip.h>

#include "freertos/FreeRTOS.h"

namespace ringLights {

#define NUM_LEDS CONFIG_LED_STRIP_NUM
#define DATA_PIN CONFIG_LED_STRIP_GPIO

#define EFFECT_TRANSITION_TICKS 1000 / portTICK_PERIOD_MS

    // This way you could set some basic colors, but no patterns
    // Do NOT re-order this list, only add new effects between the current last one and EFFECT_MAX
    enum RingLightEffect {
        /**
         * @brief Approximate a point on an angle
         *
         * @param param_a: [int] angle center of point in degrees
         * @param param_b: [int] angle of width of point in degrees
         *
         * @param primary_color: base color, fades at from center
         */
        POINTER,

        /**
         * @brief Fills a bar around the smartknob from start to percentage
         *
         * @param param_a: [int]  starting angle, between 180 and 359
         * @param param_b: [int]  ending angle, between 0 and 179
         * @param param_c: [int]  percentage from 0 to 100
         *
         * @param primary_color: base color
         * @param secondary_color: gradient color if provided (any non-zero)
         */
        PERCENT,

        /**
         * @brief Fills all LED's with the same color
         *
         * @param primary_color: base color
         */
        FILL,

        /**
         * @brief Static gradient
         *
         * @param param_a: [int]  gradient angle, between 0 and 359
         * @param param_b: [int]  gradient width, between 0 and 100 (percent)
         * @param param_c: [int]  gradient center, between 0 and 100 (percent) (0 corresponding to the bottom and 100 to the top)
         *
         * @param primary_color: base color
         * @param secondary color: gradient color
         */
        GRADIENT,

        /**
         * @brief Set every other led to another color
         *
         * @param primary_color: color for led's on even index
         * @param secondary_color: color for led's on odd index
         */
        SKIP,

        /**
         * @brief Cycles all LED's through a rainbow at the same time
         */
        RAINBOW_UNIFORM,

        /**
         * @brief Moves a rainbow around the ring
         *
         * @param param_a: [int] rotation per tick in degrees
         */
        RAINBOW_RADIAL,

        EFFECT_MAX // Leave this at the bottom
    };

    struct effectMsg {
        RingLightEffect effect = RAINBOW_UNIFORM;
        int16_t         paramA = 0, paramB = 0, paramC = 0;
        hsv_t           primaryColor{{0}, {0}, {0}}, secondaryColor{{0}, {0}, {0}};
    };

    struct brightnessMsg {
        uint8_t brightness = 127;
    };

    typedef void (*effectFunc)(rgb_t (&)[NUM_LEDS], effectMsg&);

}; // namespace ringLights

#endif /* RING_LIGHTS_DECLARATIONS_HPP */