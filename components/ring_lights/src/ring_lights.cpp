#include "ring_lights.hpp"

namespace sdk {

res ring_lights::get_status() {
    return Ok(m_status);
}

res ring_lights::initialize() {
    led_strip_install();
    return Ok(INITIALIZING);
}

res ring_lights::run() {

    static const rgb_t m_colors[] = {
    { .r = 0x0f, .g = 0x0f, .b = 0x0f },
    { .r = 0x00, .g = 0x00, .b = 0x2f },
    { .r = 0x00, .g = 0x2f, .b = 0x00 },
    { .r = 0x2f, .g = 0x00, .b = 0x00 },
    { .r = 0x00, .g = 0x00, .b = 0x00 },
};

    esp_err_t err = led_strip_fill(&m_strip, 0, NUM_LEDS, m_colors[m_color_i]);
    if (err) {
        return Err(etl::string<128>("Failed to set led strip buffer to color"));
    }

    err = led_strip_flush(&m_strip);
    if (err) {
        Err(etl::string<128>("Failed to flush led strip with buffer"));
    }

    m_color_i++;

    return Ok(RUNNING);
}

res ring_lights::stop() {
    led_strip_free(m_strip);
    return Ok(STOPPED);
}

} /* namespace sdk */
