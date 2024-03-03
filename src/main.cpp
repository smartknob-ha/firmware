#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include <inttypes.h>
#include "esp_log.h"

#include "mock_component.hpp"
#include "manager.hpp"

#include "ring_lights.hpp"
#include "light_sensor.hpp"
#include "strain_sensor.hpp"

[[noreturn]] void start_smartknob(void) {

	ring_lights::ring_lights ring_lights_;
    light_sensor::light_sensor light_sensor_;
    strain_sensor::strain_sensor strain_sensor_;
    sdk::manager::instance().add_component(ring_lights_);
    sdk::manager::instance().add_component(light_sensor_);
    sdk::manager::instance().add_component(strain_sensor_);

	sdk::manager::instance().start();

	// This is here for show, remove it if you want
	ring_lights::effect_msg msg;
	msg.primary_color = {.hue = HUE_PINK, .saturation = 255, .value = 200};
	msg.secondary_color = {.hue = HUE_YELLOW, .saturation = 255, .value = 200};
	msg.effect = ring_lights::RAINBOW_RADIAL;
	msg.param_a = 10;
	ring_lights_.enqueue(msg);

	bool flip_flop = true;
    int count = 0;
	for(;;) {
	    if (count++ > 10) {
	        if (auto light = light_sensor_.read_light_level(); light.isOk()) {
	            ESP_LOGI("main", "light value: %ld", light.unwrap());
	        }
	        if (auto strain = strain_sensor_.read_light_level(); strain.isOk()) {
	            ESP_LOGI("main", "strain value: %ld", strain.unwrap());
	        }
	        count = 0;
	    }

		if (flip_flop) {
			msg.param_a += 10;
		} else {
			msg.param_a -= 10;
		}

		if (msg.param_a == 0) {
			flip_flop = true;
		} else if (msg.param_a == 360) {
			flip_flop = false;
		}

		ring_lights_.enqueue(msg);
		vTaskDelay(pdMS_TO_TICKS(100));
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