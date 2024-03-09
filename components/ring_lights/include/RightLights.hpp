#ifndef LED_STRIP_HPP
#define LED_STRIP_HPP

#include <led_strip.h>

#include "Component.hpp"
#include "Declaration.hpp"
#include "Effects.hpp"

namespace ringLights {

#if defined(CONFIG_SK6812)
#define LED_TYPE led_strip_type_t::LED_STRIP_SK6812
#elif defined(CONFIG_WS2812)
#define LED_TYPE led_strip_type_t::LED_STRIP_WS2812
#elif defined(CONFIG_APA106)
#define LED_TYPE led_strip_type_t::LED_STRIP_APA106
#elif defined(CONFIG_SM16703)
#define LED_TYPE led_strip_type_t::LED_STRIP_SM16703
#else
#error Please define a valid LED type using menuconfig
#endif

    class RingLights : public Component,
                       public HasQueue<1, effectMsg, 0>,
                       public HasQueue<1, brightnessMsg, 0> {
    public:
        RingLights()  = default;
        ~RingLights() = default;

        /* Component override functions */
        etl::string<50> getTag() override { return TAG; };
        res             getStatus() override;
        res             initialize() override;
        res             run() override;
        res             stop() override;

        void enqueue(effectMsg& msg) override { effectMsgQueue::enqueue(msg); }
        void enqueue(brightnessMsg& msg) override { brightnessMsgQueue::enqueue(msg); }

    private:
        using effectMsgQueue     = HasQueue<1, effectMsg, 0>;
        using brightnessMsgQueue = HasQueue<1, brightnessMsg, 0>;

        static const inline char TAG[] = "Ring lights";

        rgb_t    m_currentEffectBuffer[NUM_LEDS]{};
        rgb_t    m_newEffectBuffer[NUM_LEDS]{};
        uint32_t m_effectTransitionTicksLeft = 0;

        ComponentStatus m_status = ComponentStatus::UNINITIALIZED;
        etl::string<128> m_errStatus;
        bool             m_run = false;


        effectMsg m_currentEffect{
                .effect         = RAINBOW_UNIFORM,
                .paramA         = 0,
                .paramB         = 0,
                .primaryColor   = {.h = 0, .s = 0, .v = 0},
                .secondaryColor = {.h = 0, .s = 0, .v = 0}};

        effectMsg m_newEffect;

        static void startFlush(void*);
        void        flushThread();
        void        transitionEffect();

        effectFunc m_currentEffectFunc_p = &effects::rainbowUniform;
        effectFunc m_newEffectFunc_p     = &effects::rainbowUniform;

        inline static led_strip_t m_strip = {
                .type       = LED_TYPE,
                .isRgbw     = false,
                .brightness = CONFIG_LED_MAX_BRIGHTNESS,
                .length     = NUM_LEDS,
                .gpio       = gpio_num_t(DATA_PIN),
                .channel    = (rmt_channel_t) CONFIG_LED_RMT_CHANNEL,
                .buf        = nullptr};
    };

} // namespace ringLights


#endif /* LED_STRIP_HPP */