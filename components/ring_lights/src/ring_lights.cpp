#include "ring_lights.hpp"

#include "esp_err.h"
#include "esp_log.h"

namespace sdk {

#define FLUSH_TASK_DELAY_MS 1000 / CONFIG_LED_STRIP_REFRESH_RATE

res ring_lights::get_status() {
    return Ok(m_status);
}

res ring_lights::initialize() {
    led_strip_install();
    esp_err_t err = led_strip_init(&m_strip);
    if (err) {
        return ERR("Failed to init led strip: ", err);
    }

    ESP_LOGI(TAG, "Starting ring lights");
    m_run = true;
    xTaskCreate(start_flush, "ring lights", 4096, this, 99, NULL);
    return Ok(INITIALIZING);
}

void ring_lights::start_flush(void* _this) {
    ring_lights* m = (ring_lights*) (_this);
    m->flush();
}

void ring_lights::flush() {
    ESP_LOGI(TAG, "Hello from ring lights thread!");
    uint8_t i = 0;
    while (m_run) {
        // This creates a rainbow animation at 60FPS
        // Obviously temporary until
        // https://github.com/smartknob-ha/sdk/issues/13
        // has been closed
        i ++;
        hsv_t color {
            .hue = i,
            .saturation = 255,
            .value = 255
        };
        led_strip_fill(&m_strip, 0, NUM_LEDS, hsv2rgb_rainbow(color));

        esp_err_t err = led_strip_flush(&m_strip);
        if (err) {
            ESP_LOGE(TAG, "Failed to flush led strip");
        }
        vTaskDelay(pdMS_TO_TICKS(FLUSH_TASK_DELAY_MS));
    }
}

res ring_lights::run() {
    return Ok(RUNNING);
}

res ring_lights::stop() {
    m_run = false;

    led_strip_free(&m_strip);
    return Ok(STOPPED);
}

} /* namespace sdk */
