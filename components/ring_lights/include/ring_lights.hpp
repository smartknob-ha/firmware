#ifndef LED_STRIP_HPP
#define LED_STRIP_HPP

#include <led_strip.h>
// #include <lib8tion.h>
// #include <framebuffer.h>
// #include <fbanimation.h>

// #include <effects/noise.h>
// #include <effects/plasma_waves.h>
// #include <effects/rainbow.h>
// #include <effects/waterfall.h>
// #include <effects/dna.h>
// #include <effects/rays.h>
// #include <effects/crazybees.h>
// #include <effects/sparkles.h>
// #include <effects/matrix.h>
// #include <effects/rain.h>
// #include <effects/fire.h>

#include "component.hpp"

namespace sdk {

#define NUM_LEDS CONFIG_LED_STRIP_NUM
#define DATA_PIN CONFIG_LED_STRIP_GPIO
#define CLOCK_PIN -1

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

class ring_lights : public component {
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
        static const inline char TAG[] = "LED Strip";
        COMPONENT_STATUS m_status = UNINITIALIZED;
        bool m_run = false;

        static void start_flush(void*);
        void flush();

        inline static led_strip_t m_strip = {
            .type = LED_TYPE,
            .is_rgbw = false,
            .brightness = 255,
            .length = NUM_LEDS,
            .gpio = gpio_num_t(DATA_PIN),
            .channel = RMT_CHANNEL_0,
            .buf = NULL
        };

        int m_color_i = 0;

};

} /* namespace sdk */


#endif /* LED_STRIP_HPP */