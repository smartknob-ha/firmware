#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "../sdk/manager/include/manager.hpp"
#include "../sdk/mock_component/include/mock_component.hpp"

void start_smartknob(void) {
    sdk::mock_component test_component;
    sdk::manager::instance().add_component(test_component);
    sdk::manager::instance().start();
}

extern "C" {

void app_main(void) {
    printf("Hello world!\n");

    printf("Sleeping for 5 seconds before boot...");
    sleep(5);

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    start_smartknob();
}

} /* extern "C" */