#include "StrainSensor.hpp"

#include <lowpass_filter.hpp>

#include <cmath>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system_error.hpp"

using Status = sdk::Component::Status;

signed long StrainSensor::Config::getStrainValue(StrainLevel level) const {
    switch (level) {
        case StrainLevel::RESTING:
            return restingValue.value();
        case StrainLevel::LIGHT_PRESS:
            return lightPressOffsetValue.value();
        case StrainLevel::HARD_PRESS:
            return hardPressOffsetValue.value();
        case StrainLevel::MAX:
        default:
            return UINT32_MAX;
    };
}

void StrainSensor::Config::updateField(StrainSensor::StrainLevel level, signed long value) {
    switch (level) {
        case StrainLevel::RESTING:
            Base::updateField(restingValue, value);
            break;
        case StrainLevel::LIGHT_PRESS:
            Base::updateField(lightPressOffsetValue, value);
            break;
        case StrainLevel::HARD_PRESS:
            Base::updateField(hardPressOffsetValue, value);
            break;
        case StrainLevel::MAX:
            break;
    }
}

Status StrainSensor::getStatus() {
    return m_status;
}

Status StrainSensor::initialize() {
    esp_err_t err;
    m_status = Status::INITIALIZING;
    ESP_LOGI(TAG, "Setting up hx711 strain sensor component");
    ESP_LOGI(TAG, "Current Settings: \n\tNoise: %ld\n\tResting: %ld\n\tLight press: %ld\n\tHard press: %ld",
             m_config.strainNoiseValue.value(), m_config.restingValue.value(), m_config.lightPressOffsetValue.value(), m_config.hardPressOffsetValue.value());

    gpio_config_t io_conf = {};
    io_conf.intr_type     = GPIO_INTR_DISABLE;
    io_conf.mode          = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask  = (1ULL << m_hx711_dev.pd_sck);
    io_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en    = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << m_hx711_dev.dout);
    io_conf.mode         = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    err = hx711_init(&m_hx711_dev);
    if (err) {
        ESP_LOGE(TAG, "Failed to initalize hx711: %s", esp_err_to_name(err));
    }

    // Average filter resting value over 30 minutes
    m_restingFilter.set_time_constant(1800);

    return m_status = Status::RUNNING;
}

Status StrainSensor::stop() {
    if (auto err = hx711_power_down(&m_hx711_dev, true)) {
        ESP_LOGE(TAG, "Failed to power down device: %s", esp_err_to_name(err));
    }

    return m_status = Status::STOPPED;
}

Status StrainSensor::run() {
    auto val = readStrainLevel();
    if (val.has_value()) {
        m_filteredRestingLevel = m_restingFilter.update(val.value());
    }

    return m_status;
}

std::expected<StrainSensor::StrainState, std::error_code> StrainSensor::getPressState() {
    auto strainLevel = readStrainLevel();
    if (!strainLevel.has_value()) {
        return std::unexpected(strainLevel.error());
    }

    StrainState state;

    if (m_filteredRestingLevel - strainLevel.value() > m_config.hardPressOffsetValue.value()) {
        state.level = StrainLevel::HARD_PRESS;
    } else if (m_filteredRestingLevel - strainLevel.value() > m_config.lightPressOffsetValue.value()) {
        state.level = StrainLevel::LIGHT_PRESS;
    } else {
        state.level = StrainLevel::RESTING;
    }

    float configRestingValue = static_cast<float>(m_config.restingValue.value());
    float currentValue       = static_cast<float>(strainLevel.value());

    state.percentage = static_cast<uint8_t>((currentValue - configRestingValue) / configRestingValue * 100.f);
    return state;
}

std::expected<signed long, std::error_code> StrainSensor::readStrainLevel() {
    if (m_status != Status::RUNNING) {
        return std::unexpected(std::make_error_code(static_cast<esp_err_t>(ESP_ERR_INVALID_STATE)));
    }

    if (auto err = hx711_wait(&m_hx711_dev, 150)) {
        ESP_LOGE(TAG, "Failed wait for strain sensor: %s", esp_err_to_name(err));
        return std::unexpected(std::make_error_code(err));
    }

    signed long data = 0;
    if (auto err = hx711_read_data(&m_hx711_dev, &data)) {
        ESP_LOGE(TAG, "Failed read to strain sensor value: %s", esp_err_to_name(err));
        return std::unexpected(std::make_error_code(err));
    }

    return data;
}

std::expected<signed long, std::error_code> StrainSensor::readAverageStrainLevel(size_t samples) {
    signed long data;
    if (m_status != Status::RUNNING) {
        return std::unexpected(std::make_error_code(static_cast<esp_err_t>(ESP_ERR_INVALID_STATE)));
    }

    if (auto err = hx711_read_average(&m_hx711_dev, samples, &data)) {
        ESP_LOGE(TAG, "Failed to read average strain sensor value: %s", esp_err_to_name(err));
        return std::unexpected(std::make_error_code(err));
    }

    return data;
}

std::error_code StrainSensor::saveConfig() {
    if (auto err = m_config.save()) {
        ESP_LOGW(TAG, "Unable to save config: %s - %s", esp_err_to_name(err.value()), err.message().c_str());
        return err;
    }

    return std::make_error_code(ESP_OK);
}

std::error_code StrainSensor::resetConfig() {
    if (auto err = m_config.reset()) {
        ESP_LOGE(TAG, "Unable to reset config: %s - %s", esp_err_to_name(err.value()), err.message().c_str());
        return err;
    }

    return std::make_error_code(ESP_OK);
}

std::error_code StrainSensor::calibrateNoiseValue(bool save) {
    ESP_LOGD(TAG, "calibrating noise value");
    ESP_LOGI(TAG, "Current noise value: %lu", m_restingStateNoise);

    constexpr auto num_measurements = CONFIG_STRAIN_SENSOR_NUM_CALIBRATION_MEASUREMENTS;

    etl::array<signed long, num_measurements> measurements{};
    for (auto i = 0; i < num_measurements; i++) {
        auto data = readStrainLevel();
        if (!data.has_value()) {
            return data.error();
        }
        measurements[i] = data.value();
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    auto min_max        = std::minmax_element(measurements.begin(), measurements.end());
    m_restingStateNoise = (*min_max.second - *min_max.first);
    m_config.updateField(m_config.strainNoiseValue, m_restingStateNoise);

    ESP_LOGI(TAG, "Strain noise: %lu", m_restingStateNoise);

    if (save) {
        return saveConfig();
    }

    return std::make_error_code(ESP_OK);
}

std::expected<StrainSensor::CalibrationState, std::error_code> StrainSensor::getCalibrationState() {
    if (m_calibrationError) {
        return std::unexpected(m_calibrationError);
    }
    return m_calibrationStatus;
}

void StrainSensor::calibrateValue(StrainSensor::StrainLevel strainLevel, bool save) {
    m_calibrationStatus = CalibrationState::WAITING_FOR_TRESHOLD_PASS;
    m_calibrationError  = {};

    signed long threshold;
    if (strainLevel > StrainLevel::RESTING) {
        threshold = m_config.getStrainValue(static_cast<StrainLevel>(strainLevel - 1));
    } else {
        threshold = INT32_MIN;
    }

    // We might need to retry this multiple times if the average press value isn't low enough
    bool calibrationComplete = false;
    while (!calibrationComplete) {
        // Wait for press to go above threshold
        if (strainLevel > StrainLevel::RESTING) {
            ESP_LOGI(TAG, "Waiting for input to go above %s (%ld)", LevelToString[strainLevel - 1].c_str(), threshold);

            signed long level = INT32_MIN;
            // Scale required pressure difference between the levels based on the level
            // so from resting to light press requires less effort than from light to hard press
            while (level <= threshold + (m_restingStateNoise * strainLevel)) {
                auto data = readStrainLevel();
                if (!data.has_value()) {
                    ESP_LOGE(TAG, "Failed waiting for input, trying again");
                    m_calibrationError  = data.error();
                    m_calibrationStatus = CalibrationState::ERROR;
                    break;
                }
                level = data.value();
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            ESP_LOGI(TAG, "Sufficient strain input detected [%ld], don't let go until next log message", level);
        } else {
            ESP_LOGI(TAG, "Calibrating resting value, no touchies until next log message");
        }
        m_calibrationStatus = CalibrationState::ACTIVE;

        // Start calibration
        auto data = readAverageStrainLevel(CONFIG_STRAIN_SENSOR_NUM_CALIBRATION_MEASUREMENTS);
        if (!data.has_value()) {
            m_calibrationError  = data.error();
            m_calibrationStatus = CalibrationState::ERROR;
            return;
        }

        ESP_LOGI(TAG, "Measured %s value: %ld", LevelToString[strainLevel].c_str(), data.value());

        // Higher strain levels need higher separation to avoid accidental hard presses
        if (data.value() <= threshold + (m_restingStateNoise * strainLevel)) {
            ESP_LOGI(TAG, "Please try again while pressing a *little* bit harder");
        } else {
            m_config.updateField(strainLevel, data.value() );

            if (save) {
                if (const auto err = saveConfig()) {
                    m_calibrationError  = err;
                    m_calibrationStatus = CalibrationState::ERROR;
                }
                return;
            }

            m_calibrationError  = {};
            m_calibrationStatus = CalibrationState::FINISHED;
            calibrationComplete = true;
        }
    }
}
