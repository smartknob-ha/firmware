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

std::expected<unsigned long, std::error_code> StrainSensor::readStrainLevel() {
    int32_t data;
    if (m_status != Status::RUNNING) {
        return std::unexpected(std::make_error_code(static_cast<esp_err_t>(ESP_ERR_INVALID_STATE)));
    }

    hx711_wait(&m_hx711_dev, 150);

    esp_err_t err = hx711_read_data(&m_hx711_dev, &data);

    if (err) {
        ESP_LOGE(TAG, "Failed to read ambient light value: %s", esp_err_to_name(err));
        return std::unexpected(std::make_error_code(err));
    }

    return data;
}
