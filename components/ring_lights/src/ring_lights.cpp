#include "ring_lights.hpp"
#include "effects.hpp"

#include <math.h>

#include "esp_err.h"
#include "esp_log.h"

namespace ring_lights {

#define FLUSH_TASK_DELAY_MS 1000 / CONFIG_LED_STRIP_REFRESH_RATE

res ring_lights::get_status() {
	return Ok(m_status);
}

res ring_lights::initialize() {
	led_strip_install();
	esp_err_t err = led_strip_init(&m_strip);
	if (err) {
		RETURN_ON_ERR_MSG(err, "Failed to init led strip: ");
	}

	ESP_LOGI(TAG, "Starting ring lights");
	m_run = true;
	xTaskCreatePinnedToCore(start_flush, "ring lights", 4096, this, 99, NULL, 0);
	return Ok(INITIALIZING);
}

void ring_lights::start_flush(void* _this) {
	auto* m = (ring_lights*) (_this);
	m->m_status = INITIALIZING;
	m->flush_thread();
}

void ring_lights::flush_thread() {
	ESP_LOGI(TAG, "Hello from ring lights thread!");
	m_status = RUNNING;

	while (m_run) {
		m_current_effect_func_p(m_current_effect_buffer, m_current_effect);
		if (m_effect_transition_ticks_left > 0) {
			ESP_LOGI(TAG, "m_effect_transition_ticks_left: %lu", m_effect_transition_ticks_left);
			// Fill new_effect_buffer, merge with current effect buffer
			m_new_effect_func_p(m_new_effect_buffer, m_new_effect);
			transition_effect();
			ESP_LOGI(TAG, "Transition to new effect");
		}

		led_strip_set_pixels(&m_strip, 0, NUM_LEDS, m_current_effect_buffer);
		esp_err_t err = led_strip_flush(&m_strip);
		if (err) {
			ESP_LOGE(TAG, "Failed to flush led strip");
		}
		vTaskDelay(pdMS_TO_TICKS(FLUSH_TASK_DELAY_MS));
	}
}

void ring_lights::transition_effect() {
	// As time goes on, the values in the new_effect_buffer will be weighted more
	// heavily. This is nonlinear because human eyes aren't either.
	double transition_progress = ((double) EFFECT_TRANSITION_TICKS - m_effect_transition_ticks_left) / (double) EFFECT_TRANSITION_TICKS * 100;
	transition_progress = std::max((double) 0, (std::log(transition_progress) / 2) + 1) * 255;
	ESP_LOGI(TAG, "transition_progress %f - %lu", transition_progress, m_effect_transition_ticks_left);

	for (uint8_t i = 0; i < NUM_LEDS; i++) {
		m_current_effect_buffer[i] = rgb_blend(m_current_effect_buffer[i], m_new_effect_buffer[i], transition_progress);
	}

	if (--m_effect_transition_ticks_left == 0) {
		ESP_LOGI(TAG, "Transition finished!");
		memcpy(&m_current_effect, &m_new_effect, sizeof(effect_msg));
		m_current_effect_func_p = m_new_effect_func_p;
		m_new_effect_func_p = nullptr;
		m_new_effect.effect = EFFECT_MAX;
	}
}

res ring_lights::run() {
	effect_msg tmp;

	if (xQueueReceive(m_queue, (void*) &tmp, 0) == pdTRUE) {
		if (tmp.effect == m_current_effect.effect) {
			ESP_LOGD(TAG, "Current effect update received!");
			m_current_effect = tmp;
		} else if (tmp.effect == m_new_effect.effect) {
			ESP_LOGD(TAG, "New effect update received!");
			m_new_effect = tmp;
		} else if (tmp.effect != m_current_effect.effect) {
			ESP_LOGI(TAG, "New effect received: %d, current effect %d", tmp.effect,
			         m_current_effect.effect);
			m_new_effect = tmp;
			m_new_effect_func_p = effects::get(tmp.effect);
			m_effect_transition_ticks_left = EFFECT_TRANSITION_TICKS;
		}
	}

	return Ok(m_status);
}

res ring_lights::stop() {
	m_run = false;

	led_strip_free(&m_strip);
	return Ok(STOPPED);
}

} /* namespace ring_lights */
