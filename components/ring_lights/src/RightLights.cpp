#include <math.h>

#include "Effects.hpp"
#include "RightLights.hpp"
#include "esp_err.h"
#include "esp_log.h"

namespace ringLights {

#define FLUSH_TASK_DELAY_MS (1000 / CONFIG_LED_STRIP_REFRESH_RATE)

    sdk::res RingLights::getStatus() {
        return Ok(m_status);
    }

    sdk::res RingLights::initialize() {
        led_strip_install();
        esp_err_t err = led_strip_init(&m_strip);
        if (err) {
            RETURN_ON_ERR_MSG(err, "Failed to init led strip: ");
        }

        ESP_LOGI(TAG, "Starting ring lights");
        m_run = true;
        xTaskCreatePinnedToCore(startFlush, "ring lights", 4096, this, 24, NULL, 0);
        return Ok(sdk::ComponentStatus::INITIALIZING);
    }

    void RingLights::startFlush(void* _this) {
        auto* m     = (RingLights*) (_this);
        m->m_status = sdk::ComponentStatus::INITIALIZING;
        m->flushThread();
    }

    void RingLights::flushThread() {
        ESP_LOGI(TAG, "Hello from ring lights thread!");
        m_status = sdk::ComponentStatus::RUNNING;

        while (m_run) {
            m_currentEffectFunc_p(m_currentEffectBuffer, m_currentEffect);
            if (m_effectTransitionTicksLeft > 0) {
                ESP_LOGD(TAG, "m_effect_transition_ticks_left: %lu", m_effectTransitionTicksLeft);
                // Fill new_effect_buffer, merge with current effect buffer
                m_newEffectFunc_p(m_newEffectBuffer, m_newEffect);
                transitionEffect();
                ESP_LOGD(TAG, "Transition to new effect");
            }

            led_strip_set_pixels(&m_strip, 0, NUM_LEDS, m_currentEffectBuffer);
            esp_err_t err = led_strip_flush(&m_strip);
            if (err) {
                ESP_LOGE(TAG, "Failed to flush led strip: %s", esp_err_to_name(err));
                m_status = sdk::ComponentStatus::STOPPING;
                snprintf(m_errStatus.data(), m_errStatus.max_size() - 1, "%s", esp_err_to_name(err));
            }
            vTaskDelay(pdMS_TO_TICKS(FLUSH_TASK_DELAY_MS));
        }
    }

    void RingLights::transitionEffect() {
        // As time goes on, the values in the new_effect_buffer will be weighted more
        // heavily. This is nonlinear because human eyes aren't either.
        double transition_progress = ((double) EFFECT_TRANSITION_TICKS - m_effectTransitionTicksLeft) / (double) EFFECT_TRANSITION_TICKS * 100;
        transition_progress        = std::max((double) 0, (std::log(transition_progress) / 2) + 1) * 255;
        ESP_LOGI(TAG, "transition_progress %f - %lu", transition_progress, m_effectTransitionTicksLeft);

        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            m_currentEffectBuffer[i] = rgb_blend(m_currentEffectBuffer[i], m_newEffectBuffer[i], transition_progress);
        }

        if (--m_effectTransitionTicksLeft == 0) {
            ESP_LOGI(TAG, "Transition finished!");
            memcpy(&m_currentEffect, &m_newEffect, sizeof(effectMsg));
            m_currentEffectFunc_p = m_newEffectFunc_p;
            m_newEffectFunc_p     = nullptr;
            m_newEffect.effect    = EFFECT_MAX;
        }
    }

    sdk::res RingLights::run() {
        effectMsg effect_msg_;
        if (effectMsgQueue::dequeue(effect_msg_, 0) == pdTRUE) {
            if (effect_msg_.effect == m_currentEffect.effect) {
                ESP_LOGD(TAG, "Current effect update received!");
                m_currentEffect = effect_msg_;
            } else if (effect_msg_.effect == m_newEffect.effect) {
                ESP_LOGD(TAG, "New effect update received!");
                m_newEffect = effect_msg_;
            } else if (effect_msg_.effect != m_currentEffect.effect) {
                ESP_LOGI(TAG, "New effect received: %d, current effect %d", effect_msg_.effect,
                         m_currentEffect.effect);
                m_newEffect                 = effect_msg_;
                m_newEffectFunc_p           = effects::get(effect_msg_.effect);
                m_effectTransitionTicksLeft = EFFECT_TRANSITION_TICKS;
            }
        }

        brightnessMsg brightnessMsg_;
        if (brightnessMsgQueue::dequeue(brightnessMsg_, 0) == pdTRUE) {
            m_strip.brightness = brightnessMsg_.brightness;
        }

        if (m_status == sdk::ComponentStatus::RUNNING) {
            return Ok(sdk::ComponentStatus::RUNNING);
        } else {
            return Err(m_errStatus);
        }
    }

    sdk::res RingLights::stop() {
        m_run = false;

        led_strip_free(&m_strip);
        return Ok(sdk::ComponentStatus::STOPPED);
    }

} // namespace ringLights
