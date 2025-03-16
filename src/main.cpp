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
void lvgl_task(void*) {
    while (1) {
        mutex.lock();
        uint32_t time_till_next = lv_timer_handler();
        mutex.unlock();
        // Sleep for a maximum time of CONFIG_LV_DEF_REFR_PERIOD
        vTaskDelay(std::min(time_till_next, static_cast<uint32_t>(CONFIG_LV_DEF_REFR_PERIOD)) / portTICK_PERIOD_MS);
    }
}

void calibrateStrainSensor(StrainSensor& strainSensor) {
    auto column_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(column_, 240, 240);
    lv_obj_set_flex_flow(column_, LV_FLEX_FLOW_COLUMN);

    static lv_style_t style_indic;
    lv_style_init(&style_indic);
    lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
    lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_HOR);

    lv_obj_t * label1 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(label1, lv_color_black(), LV_PART_MAIN);
    lv_label_set_long_mode(label1, LV_LABEL_LONG_WRAP);     /*Break the long lines*/

    lv_obj_set_width(label1, 200);  /*Set smaller width to make the lines wrap*/
    lv_obj_set_style_text_align(label1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label1, LV_ALIGN_CENTER, 0, -40);

    lv_label_set_text(label1, "Sample text");
    lv_task_handler();

    lv_obj_t * label2 = lv_label_create(lv_scr_act());
    lv_label_set_long_mode(label2, LV_LABEL_LONG_WRAP); /* Break the long lines */
    lv_obj_set_width(label2, 150);
    lv_label_set_text(label2, "");
    lv_obj_align(label2, LV_ALIGN_CENTER, 0, 40);

    if (strainSensor.needsFirstTimeSetup()) {
        lv_label_set_text(label1, "Calibrating noise value");
        strainSensor.calibrateNoiseValue();
        lv_label_set_text(label1, "Done calibrating noise value");

        vTaskDelay(pdMS_TO_TICKS(300));
        lv_label_set_text(label1, "Calibrating resting value");
        strainSensor.calibrateValue(StrainSensor::RESTING);
        lv_label_set_text(label1, "Done calibrating!");

        vTaskDelay(pdMS_TO_TICKS(300));
        lv_label_set_text(label1, "Calibrating light press value");
        strainSensor.calibrateValue(StrainSensor::LIGHT_PRESS);
        lv_label_set_text(label1, "Done calibrating!");

        vTaskDelay(pdMS_TO_TICKS(300));
        lv_label_set_text(label1, "Calibrating hard press value");
        strainSensor.calibrateValue(StrainSensor::HARD_PRESS);
        lv_label_set_text(label1, "Done calibrating!");

        strainSensor.saveConfig();
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    lv_obj_delete_async(label1);
    lv_obj_delete_async(label2);
}

[[noreturn]] void startSmartknob(void) {
    ringLights::RingLights ringLights;
    LightSensor            lightSensor;
    MagneticEncoder        magneticEncoder;
    MotorDriver            motorDriver;
    StrainSensor           strainSensor;

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
	sdk::Manager::addComponent(strainSensor);
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

    lv_obj_t* dot = lv_led_create(lv_scr_act());
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);
    lv_led_off(dot);

    displayDriver.setBrightness(255);

    // This is here for show, remove it if you want
    ringLights::effectMsg msg;
    msg.primaryColor   = {.hue = HUE_PINK, .saturation = 255, .value = 200};
    msg.secondaryColor = {.hue = HUE_YELLOW, .saturation = 255, .value = 200};
    msg.effect         = ringLights::POINTER;
    msg.paramA         = 1.0f;
    msg.paramB         = 40;
    ringLights.enqueue(msg);

    esp_log_level_set(motorDriver.getTag().c_str(), ESP_LOG_DEBUG);
    sdk::Manager::addComponent(motorDriver);
    while (!sdk::Manager::isInitialized()) { vTaskDelay(1); };

    motorDriver.setDetentConfig(espp::detail::COARSE_VALUES_STRONG_DETENTS, 16);

    msg.primaryColor = {.hue = HUE_BLUE, .saturation = 255, .value = 200};
    msg.effect = ringLights::POINTER;

    xTaskCreatePinnedToCore(lvgl_task, "LVGL", 4096, NULL, 24, NULL, 0);

    calibrateStrainSensor(strainSensor);

    size_t count = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
        auto degrees = magneticEncoder.getDegrees();
        auto radians = magneticEncoder.getRadians();
        auto dev     = magneticEncoder.getDevice();

        if (++count > 100) {
            if (auto light = lightSensor.readLightLevel(); light.has_value()) {
                ESP_LOGI("main", "light value: %ld", light.value());
            }

            ESP_LOGI("main", "encoder degrees: %lf", degrees.value());
            ESP_LOGI("main", "raw encoder degrees: %lf", magneticEncoder.getDevice()->get()->get_degrees());

            ESP_LOGI("main", "current haptics position: %f", motorDriver.getPosition());

            ESP_LOGI("main", "strain level: %ld", strainSensor.readStrainLevel().value_or(INT32_MAX));

            count = 0;
        }

        msg.paramA = degrees.value();

        ringLights.enqueue(msg);

        auto x = static_cast<int>(100 * std::cos((magneticEncoder.getRadians().value() * -1) - M_PI_2));
        auto y = static_cast<int>(100 * std::sin((magneticEncoder.getRadians().value() * -1) - M_PI_2));
        std::scoped_lock lock{mutex};
        lv_obj_align(dot, LV_ALIGN_CENTER, x, y);
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
