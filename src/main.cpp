#include <inttypes.h>
#include <stdio.h>

#include "LightSensor.hpp"
#include "Manager.hpp"
#include "RightLights.hpp"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "DisplayDriver.hpp"

[[noreturn]] void startSmartknob(void) {

    ringLights::RingLights ringLights;
    LightSensor            lightSensor;

    DisplayDriver::Config displayConfig{
            .display_dc        = GPIO_NUM_16,
            .display_reset     = GPIO_NUM_4,
            .display_backlight = GPIO_NUM_7,
            .spi_sclk          = GPIO_NUM_5,
            .spi_mosi          = GPIO_NUM_6,
            .spi_cs            = GPIO_NUM_15,
            .rotation          = DisplayRotation::LANDSCAPE};
    DisplayDriver displayDriver(displayConfig);

    sdk::Manager::instance().addComponent(ringLights);
    sdk::Manager::instance().addComponent(lightSensor);
    sdk::Manager::instance().addComponent(displayDriver);

    sdk::Manager::instance().start();

    // Wait for components to actually be running
    vTaskDelay(pdMS_TO_TICKS(4000));

    displayDriver.setBrightness(255);

    // This is here for show, remove it if you want
    ringLights::effectMsg msg;
    msg.primaryColor   = {.hue = HUE_PINK, .saturation = 255, .value = 200};
    msg.secondaryColor = {.hue = HUE_YELLOW, .saturation = 255, .value = 200};
    msg.effect         = ringLights::POINTER;
    msg.paramA         = 1;
    msg.paramB         = 40;
    ringLights.enqueue(msg);

    bool flipFlop = true;
    int  count    = 0;
    for (;;) {
        if (count++ > 10) {
            if (auto light = lightSensor.readLightLevel(); light.isOk()) {
                ESP_LOGI("main", "light value: %ld", light.unwrap());
            }
            count = 0;
        }

        if (flipFlop) {
            msg.paramA += 1;
        } else {
            msg.paramA -= 1;
        }

        if (msg.paramA == 0) {
            flipFlop = true;
        } else if (msg.paramA == 1000) {
            flipFlop = false;
        }

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
