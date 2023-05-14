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
#include "effects.hpp"
#include "declarations.hpp"

// Lots of ring lights demo code in here until
// we actually have a UI director thing

static ring_lights::ring_lights ring_lights_;


void rainbow_anim_test(void*) {
	ESP_LOGI("main", "hello from rainbow_anim_test!");
	ring_lights::effect_msg msg {
		.effect = ring_lights::RAINBOW_UNIFORM,
		.param_a = 0,
		.param_b = 0,
		.primary_color = {0,255,255},
		.secondary_color = {0,255,255}
	};

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(10000));
		ring_lights_.enqueue(msg);
	}
}

void start_smartknob(void) {
	// static sdk::mock_component mock_component_;
	// sdk::manager::instance().add_component(mock_component_);
	sdk::manager::instance().add_component(ring_lights_);

	ring_lights::effect_msg msg {
		.effect = ring_lights::RAINBOW_UNIFORM,
		.param_a = 0,
		.param_b = 0,
		.primary_color = {0,0,0},
		.secondary_color = {0,0,0}
	};

	ring_lights_.enqueue(msg);
	xTaskCreate(rainbow_anim_test, "rainbow-anim-test", 4096, 0, 99, NULL);

	sdk::manager::instance().start();
	msg.primary_color = {HUE_BLUE, 0, 255};
	msg.effect = ring_lights::POINTER;
	msg.param_b = 120;
	ring_lights_.enqueue(msg);
	while (true) {
		for (int16_t angle = 360; angle >= -360; angle -= 3) {
			msg.param_a = angle;
			ring_lights_.enqueue(msg);
			vTaskDelay(pdMS_TO_TICKS(1000/CONFIG_LED_STRIP_REFRESH_RATE));
		}

		for (int16_t angle = 0; angle <= 720; angle += 3) {
			msg.param_a = angle;
			ring_lights_.enqueue(msg);
			vTaskDelay(pdMS_TO_TICKS(1000/CONFIG_LED_STRIP_REFRESH_RATE));
		}

		vTaskDelay(pdMS_TO_TICKS(13000));
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