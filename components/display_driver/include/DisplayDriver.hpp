#ifndef DISPLAY_DRIVER_HPP
#define DISPLAY_DRIVER_HPP

#include "Component.hpp"
#include "etl/string.h"
#include "driver/gpio.h"

enum class DisplayRotation : uint8_t {
    LANDSCAPE,
    PORTRAIT,
    LANDSCAPE_INVERTED,
    PORTRAIT_INVERTED
};

struct DisplayMsg {
    uint8_t brightness;
};

class DisplayDriver : public sdk::Component,
                      public sdk::HasQueue<1, DisplayMsg, 0> {
public:
    struct Config {
        gpio_num_t         display_dc;
        gpio_num_t         display_reset;
        gpio_num_t         display_backlight;
        gpio_num_t         spi_sclk;
        gpio_num_t         spi_mosi;
        gpio_num_t         spi_cs;
        DisplayRotation rotation;
    };

    DisplayDriver(Config& config) : m_config(config) {}
    ~DisplayDriver() = default;

    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };
    sdk::res        getStatus() override;
    sdk::res        initialize() override;
    sdk::res        run() override;
    sdk::res        stop() override;

    void enqueue(DisplayMsg& msg) override { DisplayMsgQueue::enqueue(msg); }

    void setBrightness(uint8_t brightness);

private:
    using DisplayMsgQueue = HasQueue<1, DisplayMsg, 0>;

    static const inline char TAG[] = "Display driver";

    sdk::ComponentStatus m_status = sdk::ComponentStatus::UNINITIALIZED;
    etl::string<128>     m_errStatus;
    bool                 m_run = false;

    Config m_config;

    inline static bool m_initialized = false;
};

#endif // DISPLAY_DRIVER_HPP
