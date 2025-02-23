#include <inttypes.h>
#include <lvgl.h>
#include <stdio.h>

#include "DisplayDriver.hpp"
#include "LightSensor.hpp"
#include "MagneticEncoder.hpp"
#include "Manager.hpp"
#include "RightLights.hpp"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "color.h"

//TODO To be placed in UI component once created
std::mutex mutex;
void lvgl_task(void*) {
    while (1) {
        mutex.lock();
        uint32_t time_till_next = lv_timer_handler();
        mutex.unlock();
        // Sleep for a maximum time of CONFIG_LV_DEF_REFR_PERIOD
        vTaskDelay(std::min(time_till_next, static_cast<uint32_t>(CONFIG_LV_DEF_REFR_PERIOD)) / portTICK_PERIOD_MS);
    }
}

[[noreturn]] void startSmartknob(void) {
    // ringLights::RingLights ringLights;
    // LightSensor            lightSensor;
    // MagneticEncoder        magneticEncoder;

    DisplayDriver::Config displayConfig{
            .display_dc        = GPIO_NUM_16,
            .display_reset     = GPIO_NUM_4,
            .display_backlight = GPIO_NUM_7,
            .spi_sclk          = GPIO_NUM_5,
            .spi_mosi          = GPIO_NUM_6,
            .spi_cs            = GPIO_NUM_15,
            .rotation          = DisplayRotation::LANDSCAPE};
    DisplayDriver displayDriver(displayConfig);

    // sdk::Manager::addComponent(ringLights);
    // sdk::Manager::addComponent(lightSensor);
    // sdk::Manager::addComponent(magneticEncoder);
    sdk::Manager::addComponent(displayDriver);

    sdk::Manager::start();

    // Wait for components to actually be running
    while (!sdk::Manager::isInitialized()) { vTaskDelay(1); };

    // auto res = magneticEncoder.getDevice();
    // if (res.has_value()) {
        //		MotorDriver motorDriver(res.value());

        //		sdk::Manager::addComponent(motorDriver);
    // } else {
        // ESP_LOGE("main", "Unable to start magnetic encoder: %s", res.error().message().c_str());
    // }

    lv_obj_t* dot = lv_led_create(lv_scr_act());
    auto test = lv_scr_act();
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);
    lv_led_off(dot);

    lv_led_set_color(dot, lv_color_hex(0x00FF00));

    displayDriver.setBrightness(255);

    //
    // This is here for show, remove it if you want
    ringLights::effectMsg msg;
    msg.primaryColor   = {.hue = HUE_PINK, .saturation = 255, .value = 200};
    msg.secondaryColor = {.hue = HUE_YELLOW, .saturation = 255, .value = 200};
    msg.effect         = ringLights::POINTER;
    msg.paramA         = 1.0f;
    msg.paramB         = 40;
    // ringLights.enqueue(msg);
    //

    xTaskCreatePinnedToCore(lvgl_task, "LVGL", 4096, NULL, 24, NULL, 0);

    size_t count = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
        // auto degrees = magneticEncoder.getDegrees();

        if (count++ > 100) {
            // if (auto light = lightSensor.readLightLevel(); light.has_value()) {
                // ESP_LOGI("main", "light value: %ld", light.value());
            // }

            // ESP_LOGI("main", "encoder degrees: %lf", degrees.value());
            count = 0;
        }

        // msg.paramA = degrees.value();

        // ringLights.enqueue(msg);

        // auto x = static_cast<int>(100 * std::cos((magneticEncoder.getRadians().value() * -1) - M_PI_2));
        // auto y = static_cast<int>(100 * std::sin((magneticEncoder.getRadians().value() * -1) - M_PI_2));
        std::scoped_lock lock{mutex};
        lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);
    }
}

extern "C" void app_main(void) {
    // ESP_LOGI("main", "\nSleeping for 5 seconds before boot...\n");
    // sleep(5);

    /* Print chip information */
    esp_chip_info_t chipInfo;
    uint32_t        flashSize;
    esp_chip_info(&chipInfo);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s%s, ",
           CONFIG_IDF_TARGET,
           chipInfo.cores,
           (chipInfo.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chipInfo.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
           (chipInfo.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned majorRev = chipInfo.revision / 100;
    unsigned minorRev = chipInfo.revision % 100;
    printf("silicon revision v%d.%d, ", majorRev, minorRev);
    if (esp_flash_get_size(NULL, &flashSize) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flashSize / (uint32_t) (1024 * 1024),
           (chipInfo.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    startSmartknob();
}
