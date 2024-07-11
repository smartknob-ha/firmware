#include <inttypes.h>
#include <stdio.h>

#include "DisplayDriver.hpp"
#include "LightSensor.hpp"
#include "MagneticEncoder.hpp"
#include "Manager.hpp"
#include "MotorDriver.hpp"
#include "RightLights.hpp"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lv_api_map.h"

[[noreturn]] void startSmartknob(void) {
    ringLights::RingLights ringLights;
    LightSensor            lightSensor;
    MagneticEncoder        magneticEncoder;
    MotorDriver            motorDriver;

    	DisplayDriver::Config displayConfig{
                .display_dc        = GPIO_NUM_16,
                .display_reset     = GPIO_NUM_4,
                .display_backlight = GPIO_NUM_7,
                .spi_sclk          = GPIO_NUM_5,
                .spi_mosi          = GPIO_NUM_6,
                .spi_cs            = GPIO_NUM_15,
                .rotation          = DisplayRotation::LANDSCAPE};
        DisplayDriver displayDriver(displayConfig);

    sdk::Manager::addComponent(ringLights);
    sdk::Manager::addComponent(lightSensor);
    sdk::Manager::addComponent(magneticEncoder);
    sdk::Manager::addComponent(displayDriver);

    sdk::Manager::start();

    // Wait for components to actually be running
    while (!sdk::Manager::isInitialized()) { vTaskDelay(1); };

    auto res = magneticEncoder.getDevice();
    if (res.has_value()) {
        motorDriver.setSensor(res.value());
    } else {
        ESP_LOGE("main", "Unable to start magnetic encoder: %s", res.error().message().c_str());
    }

    // This is here for show, remove it if you want
    ringLights::effectMsg msg;
    msg.primaryColor   = {.hue = HUE_AQUA, .saturation = 255, .value = 150};
    msg.secondaryColor = {.hue = HUE_YELLOW, .saturation = 255, .value = 200};
    msg.effect         = ringLights::RAINBOW_RADIAL;
    msg.paramA         = 5.0f;
    msg.paramB         = 40;
    ringLights.enqueue(msg);

    esp_log_level_set(motorDriver.getTag().c_str(), ESP_LOG_DEBUG);

    sdk::Manager::addComponent(motorDriver);

    while (!sdk::Manager::isInitialized()) { vTaskDelay(1); };

    motorDriver.setZero();

    displayDriver.setBrightness(255);

    msg.primaryColor = {.hue = HUE_BLUE, .saturation = 255, .value = 200};
    msg.effect = ringLights::POINTER;

    size_t count = 0;
    for (;;) {
        auto degrees = magneticEncoder.getDegrees();
        auto radians = magneticEncoder.getRadians();
        auto dev     = magneticEncoder.getDevice();

        if (++count > 100) {
            if (auto light = lightSensor.readLightLevel(); light.has_value()) {
                ESP_LOGI("main", "light value: %ld", light.value());
            }

            ESP_LOGI("main", "encoder degrees: %lf", degrees.value());
            ESP_LOGI("main", "raw encoder degrees: %lf", magneticEncoder.getDevice()->get()->get_degrees());

            ESP_LOGI("main", "encoder radians: %lf", radians.value());
            ESP_LOGI("main", "raw encoder radians: %lf", magneticEncoder.getDevice()->get()->get_radians());
            count = 0;
        }

        lv_task_handler();
        msg.paramA = degrees.value();

        ringLights.enqueue(msg);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI("main", "\nSleeping for 5 seconds before boot...\n");
    sleep(5);

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
