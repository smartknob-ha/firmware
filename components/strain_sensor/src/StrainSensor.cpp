#include <cmath>

#include "StrainSensor.hpp"

#include "esp_err.h"
#include "esp_system_error.hpp"
#include "esp_log.h"

using Status = sdk::Component::Status;
using res    = sdk::Component::res;

Status StrainSensor::getStatus() {
    return m_status;
}

Status StrainSensor::initialize() {
    esp_err_t err;
    m_status = Status::INITIALIZING;
    ESP_LOGI(TAG, "Setting up hx711 strain sensor component");

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<< m_hx711_dev.pd_sck);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL<< m_hx711_dev.dout);
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    err = hx711_init(&m_hx711_dev);
    if (err) {
        ESP_LOGE(TAG, "Failed to initalize hx711: %s", esp_err_to_name(err));
    }

    return m_status = Status::RUNNING;
}

Status StrainSensor::stop() {
    esp_err_t err = hx711_power_down(&m_hx711_dev, true);
    if (err) {
        ESP_LOGE(TAG, "Failed to power down device: %s", esp_err_to_name(err));
    }
    return m_status = Status::STOPPED;
}

std::expected<StrainSensor::pressState, std::error_code> StrainSensor::getPressState() {
    auto strainLevel = readStrainLevel();
    if (strainLevel.has_value()) {
        if (strainLevel.value() > m_config.hardPressValue.value()) {
            return pressState::HARD_PRESS;
        } else if (strainLevel.value() > m_config.lightPressValue.value()) {
            return pressState::LIGHT_PRESS;
        } else {
            return pressState::RESTING;
        }
    }

    return std::unexpected(strainLevel.error());
}

std::expected<unsigned long, std::error_code> StrainSensor::readStrainLevel() {
    if (m_status != Status::RUNNING) {
        return std::unexpected(std::make_error_code(static_cast<esp_err_t>(ESP_ERR_INVALID_STATE)));
    }

    if (auto err = hx711_wait(&m_hx711_dev, 150)) {
        ESP_LOGE(TAG, "Failed to strain sensor value: %s", esp_err_to_name(err));
        return std::unexpected(std::make_error_code(err));
    }

    int32_t data;
    if (auto err = hx711_read_data(&m_hx711_dev, &data)) {
        ESP_LOGE(TAG, "Failed to strain sensor value: %s", esp_err_to_name(err));
        return std::unexpected(std::make_error_code(err));
    }

    return data;
}

std::expected<unsigned long, std::error_code> StrainSensor::readAverageStrainLevel(size_t samples) {
    int32_t data;
    if (m_status != Status::RUNNING) {
        return std::unexpected(std::make_error_code(static_cast<esp_err_t>(ESP_ERR_INVALID_STATE)));
    }

    if (auto err = hx711_wait(&m_hx711_dev, 150)) {
        ESP_LOGE(TAG, "Failed to strain sensor value: %s", esp_err_to_name(err));
        return std::unexpected(std::make_error_code(err));
    }

    if (auto err = hx711_read_average(&m_hx711_dev, samples, &data)) {
        ESP_LOGE(TAG, "Failed to strain sensor value: %s", esp_err_to_name(err));
        return std::unexpected(std::make_error_code(err));
    }

    return data;
}

std::error_code StrainSensor::saveConfig() {
    if (auto err = m_config.save()) {
        ESP_LOGW(TAG, "Unable to save config: %s", esp_err_to_name(err.value()));
        return err;
    }
    return std::make_error_code(ESP_OK);
}

std::error_code StrainSensor::calibrateRestingValue(bool save) {
    ESP_LOGD(TAG, "calibrating resting value");
    ESP_LOGI(TAG, "Current resting value: %lu", m_config.restingValue.value());

    auto data = readAverageStrainLevel(1000);
    if (data.has_value()) {
        // Voltage goes down when the sensors are stretched
        if (data.value() < m_config.lightPressValue.value()) {
            ESP_LOGE(TAG, "Value lower than light press value, make sure to not press down while calibrating");
            return std::make_error_code(ESP_ERR_INVALID_ARG);
        }

        m_config.updateField(m_config.restingValue, data.value());
        ESP_LOGI(TAG, "Measured resting value: %lu", m_config.restingValue.value());

        if (save) {
            return saveConfig();
        }

        return std::make_error_code(ESP_OK);
    }

    return data.error();
}

std::error_code StrainSensor::calibrateLightPressValue(bool save) {
    ESP_LOGD(TAG, "Calibrating light press value");
    ESP_LOGI(TAG, "Current light pressure value: %lu", m_config.lightPressValue.value());

    auto data = readAverageStrainLevel(1000);
    if (data.has_value()) {
        // Voltage goes down when the sensors are stretched
        if (data.value() < m_config.hardPressValue.value()) {
            ESP_LOGE(TAG, "Value lower than hard press value, should be higher");
            return std::make_error_code(ESP_ERR_INVALID_ARG);
        } else if (data.value() > m_config.restingValue.value()) {
            ESP_LOGE(TAG, "Value higher than resting value, should be lower");
            return std::make_error_code(ESP_ERR_INVALID_ARG);
        }

        m_config.updateField(m_config.lightPressValue, data.value());
        ESP_LOGI(TAG, "Measured light press value: %lu", m_config.lightPressValue.value());

        if (save) {
            return saveConfig();
        }

        return std::make_error_code(ESP_OK);
    }

    return data.error();
}

std::error_code StrainSensor::calibrateHardPressValue(bool save) {
    ESP_LOGD(TAG, "Calibrating hard press value");
    ESP_LOGI(TAG, "Current hard press value: %lu", m_config.hardPressValue.value());

    auto data = readAverageStrainLevel(1000);
    if (data.has_value()) {
        // Voltage goes down when the sensors are stretched
        if (data.value() > m_config.lightPressValue.value()) {
            ESP_LOGE(TAG, "Value higher than light press value, should be lower");
            return std::make_error_code(ESP_ERR_INVALID_ARG);
        }

        m_config.updateField(m_config.hardPressValue, data.value());
        ESP_LOGI(TAG, "Measured hard press value: %lu", m_config.hardPressValue.value());

        if (save) {
            return saveConfig();
        }

        return std::make_error_code(ESP_OK);
    }

    return data.error();
}
