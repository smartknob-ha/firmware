#ifndef DISPLAY_DRIVER_HPP
#define DISPLAY_DRIVER_HPP

#include "Component.hpp"
#include "driver/gpio.h"
#include "etl/string.h"

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
        gpio_num_t      display_dc;
        gpio_num_t      display_reset;
        gpio_num_t      display_backlight;
        gpio_num_t      spi_sclk;
        gpio_num_t      spi_mosi;
        gpio_num_t      spi_cs;
        DisplayRotation rotation;
    };

    DisplayDriver(Config& config) : m_config(config) {}
    ~DisplayDriver() = default;

    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };
    Status          initialize() override;
    Status          run() override;
    Status          stop() override;

    void enqueue(DisplayMsg& msg) override { DisplayMsgQueue::enqueue(msg); }

    void setBrightness(uint8_t brightness);

private:
    using DisplayMsgQueue = HasQueue<1, DisplayMsg, 0>;

    static const inline char TAG[] = "Display driver";

    Status           m_status = Status::UNINITIALIZED;
    etl::string<128> m_errStatus;
    bool             m_run = false;

    Config m_config;

    inline static bool m_initialized = false;

    static void waitForLines();

    static void IRAM_ATTR sendLines(int xs, int ys, int xe, int ye, const uint8_t* data, uint32_t user_data);
};

#endif // DISPLAY_DRIVER_HPP
