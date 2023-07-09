#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include <inttypes.h>
#include "esp_log.h"

#include "../sdk/mock_component/include/mock_component.hpp"
#include "../sdk/manager/include/manager.hpp"

#include "ring_lights.hpp"

void start_smartknob(void) {

	ring_lights::ring_lights ring_lights_;
	sdk::manager::instance().add_component(ring_lights_);

	sdk::manager::instance().start();

	// This is here for show, remove it if you want
	ring_lights::effect_msg msg;
	msg.primary_color = {.hue = HUE_AQUA, .saturation = 255, .value = 200};
	msg.secondary_color = {.hue = HUE_YELLOW, .saturation = 255, .value = 200};
	msg.effect = ring_lights::GRADIENT;
	msg.param_a = 0;
	msg.param_b = 75;
	msg.param_c = 75;
	ring_lights_.enqueue(msg);


	bool flipflop = true;
	while (true) {

		if (flipflop) {
			msg.param_b -= 10;
			if (msg.param_b == 0) {
				flipflop = false;
			}
		} else {
			msg.param_b += 10;
			if (msg.param_b == 100) {
				flipflop = true;
			}
		}

		ring_lights_.enqueue(msg);

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

extern "C" {

void app_main(void) {
	ESP_LOGI("main", "\nSleeping for 5 seconds before boot...\n");
	sleep(5);

	 /* Print chip information */
	esp_chip_info_t chip_info;
	uint32_t flash_size;
	esp_chip_info(&chip_info);
	printf("This is %s chip with %d CPU core(s), WiFi%s%s%s, ",
		   CONFIG_IDF_TARGET,
		   chip_info.cores,
		   (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
		   (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
		   (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

	unsigned major_rev = chip_info.revision / 100;
	unsigned minor_rev = chip_info.revision % 100;
	printf("silicon revision v%d.%d, ", major_rev, minor_rev);
	if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
		printf("Get flash size failed");
		return;
	}

	printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
		   (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

	printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

	start_smartknob();
}

}