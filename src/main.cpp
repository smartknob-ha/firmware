#include <inttypes.h>
#include <lvgl.h>
#include <stdio.h>

#include "DisplayDriver.hpp"
#include "LightSensor.hpp"
#include "MagneticEncoder.hpp"
#include "Manager.hpp"
#include "MotorDriver.hpp"
#include "RightLights.hpp"
#include "StrainSensor.hpp"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//TODO To be placed in UI component once created
std::mutex mutex;

[[noreturn]] void lvgl_task(void*) {
    while (1) {
        mutex.lock();
        uint32_t time_till_next = lv_timer_handler();
        mutex.unlock();
        // Sleep for a maximum time of CONFIG_LV_DEF_REFR_PERIOD
        vTaskDelay(std::min(time_till_next, static_cast<uint32_t>(CONFIG_LV_DEF_REFR_PERIOD)) / portTICK_PERIOD_MS);
    }
}

[[noreturn]] void startSmartknob(void) {
    ringLights::RingLights ringLights;
    LightSensor            lightSensor;
    StrainSensor           strainSensor;
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
    sdk::Manager::addComponent(strainSensor);
    sdk::Manager::addComponent(lightSensor);
    sdk::Manager::addComponent(magneticEncoder);
    sdk::Manager::addComponent(displayDriver);

    sdk::Manager::start();

    // Wait for components to actually be running
    while (!sdk::Manager::isInitialized()) { vTaskDelay(1); };

    auto res = magneticEncoder.getDevice();
    if (res.has_value()) {
        //		MotorDriver motorDriver(res.value());

        //		sdk::Manager::addComponent(motorDriver);
    } else {
        ESP_LOGE("main", "Unable to start magnetic encoder: %s", res.error().message().c_str());
    }

    lv_obj_t* dot = lv_led_create(lv_scr_act());
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);
    lv_led_off(dot);

    displayDriver.setBrightness(255);

    xTaskCreatePinnedToCore(lvgl_task, "LVGL", 4096, NULL, 24, NULL, 0);

    //
    // This is here for show, remove it if you want
    ringLights::effectMsg msg;
    msg.primaryColor   = {.hue = HUE_AQUA, .saturation = 255, .value = 150};
    msg.effect         = ringLights::RAINBOW_RADIAL;
    msg.paramA         = 2.0f;
    ringLights.enqueue(msg);

    lv_task_handler();

    sdk::Manager::addComponent(motorDriver);

    while (!sdk::Manager::isInitialized()) { vTaskDelay(1); };

    lv_label_set_text(label1, "Calibrating motor, don't touch me please...");
    lv_task_handler();
    motorDriver.setZero();

    if (strainSensor.needsFirstTimeSetup()) {
        lv_label_set_text(label1, "Calibrating noise value");
        lv_task_handler();
        strainSensor.calibrateNoiseValue();
        lv_label_set_text(label1, "Done calibrating noise value");
        lv_task_handler();

        vTaskDelay(pdMS_TO_TICKS(300));
        lv_label_set_text(label1, "Calibrating resting value");
        lv_task_handler();
        strainSensor.calibrateValue(StrainSensor::RESTING);
        lv_label_set_text(label1, "Done calibrating!");
        lv_task_handler();

        vTaskDelay(pdMS_TO_TICKS(300));
        lv_label_set_text(label1, "Calibrating light press value");
        lv_task_handler();
        strainSensor.calibrateValue(StrainSensor::LIGHT_PRESS);
        lv_label_set_text(label1, "Done calibrating!");
        lv_task_handler();

        vTaskDelay(pdMS_TO_TICKS(300));
        lv_label_set_text(label1, "Calibrating hard press value");
        lv_task_handler();
        strainSensor.calibrateValue(StrainSensor::HARD_PRESS);
        lv_label_set_text(label1, "Done calibrating!");
        lv_task_handler();

        strainSensor.saveConfig();
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    msg.primaryColor = {.hue = HUE_BLUE, .saturation = 255, .value = 255};
    msg.paramA = magneticEncoder.getDegrees().value();
    msg.effect = ringLights::POINTER;
    ringLights.enqueue(msg);


    char label2Buffer[150];

    int factoryResetCounter = 30;
    int count = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
        auto degrees = magneticEncoder.getDegrees();

        if (count++ > 100) {
            if (auto light = lightSensor.readLightLevel(); light.has_value()) {
                ESP_LOGI("main", "light value: %ld", light.value());
            }

            ESP_LOGI("main", "encoder degrees: %lf", degrees.value());
            count = 0;
        }

        msg.paramA = degrees.value();
        ringLights.enqueue(msg);

        if (++count > 25) {
//            if (auto light = lightSensor.readLightLevel(); light.has_value()) {
//                ESP_LOGI("main", "light value: %ld", light.value());
//            }
//
//            ESP_LOGI("main", "encoder degrees: %lf", degrees.value());
//            ESP_LOGI("main", "raw encoder degrees: %lf", magneticEncoder.getDevice()->get()->get_degrees());
//
//            ESP_LOGI("main", "encoder radians: %lf", radians.value());
//            ESP_LOGI("main", "raw encoder radians: %lf", magneticEncoder.getDevice()->get()->get_radians());

            auto strainLevel = strainSensor.getPressState().value();
            etl::string<30> StateString("");
            StateString += StrainSensor::LevelToString[strainLevel.level];
            ESP_LOGI("main", "strain state: %s", StateString.c_str());
            ESP_LOGI("main", "strain percentage: %d", strainLevel.percentage);
            ESP_LOGI("main", "raw strain level: %lu", strainSensor.readStrainLevel().value());

            lv_label_set_text(label1, StateString.c_str());

            if (strainLevel.level == StrainSensor::StrainLevel::HARD_PRESS) {
                snprintf(label2Buffer, 150, "Factory reset in: %d", factoryResetCounter);
                lv_label_set_text(label2, label2Buffer);
                --factoryResetCounter;
            } else {
                factoryResetCounter = 50;
                lv_label_set_text(label2, "");
            }

            if (factoryResetCounter == 0) {
                if (auto err = strainSensor.resetConfig()) {
                    ESP_LOGE("main", "Failed to reset strainSensor config");
                    abortusMaximus;
                } else {
                    ESP_LOGI("main", "\n-----\n\tREBOOTING\n-----");
                    esp_restart();
                }
            }

            count = 0;
        }

        lv_task_handler();

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
