#include "../include/LightSensor.hpp"

#include <veml7700.h>

#include <cmath>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system_error.hpp"

#define SDA_GPIO_NUM CONFIG_LIGHT_SENSOR_SDA_GPIO_NUM
#define SCL_GPIO_NUM CONFIG_LIGHT_SENSOR_SCL_GPIO_NUM

using Status = sdk::Component::Status;

Status LightSensor::getStatus() { return m_status; }

Status LightSensor::initialize() {
    m_status = Status::INITIALIZING;
    ESP_LOGI(TAG, "Setting up veml7700 light sensor component");

    memset(&m_dev, 0, sizeof(i2c_dev_t));
    auto err = std::make_error_code(i2cdev_init());
    if (err) {
        ESP_LOGE(TAG, "Failed to init i2c device: %s", err.message().c_str());
        return m_status = Status::ERROR;
    }

    err = std::make_error_code(veml7700_init_desc(&m_dev, I2C_NUM_0, static_cast<gpio_num_t>(SDA_GPIO_NUM), static_cast<gpio_num_t>(SCL_GPIO_NUM)));
    if (err) {
        ESP_LOGE(TAG, "Failed to init light sensor i2c communication: %s", err.message().c_str());
        return m_status = Status::ERROR;
    }

    err = std::make_error_code(veml7700_probe(&m_dev));
    if (err) {
        ESP_LOGE(TAG, "Failed to probe light sensor: %s", err.message().c_str());
        return m_status = Status::ERROR;
    }

    m_conf.shutdown = 0;
    err             = std::make_error_code(veml7700_set_config(&m_dev, &m_conf));
    if (err) {
        ESP_LOGE(TAG, "Failed to set light sensor config: %s", err.message().c_str());
        return m_status = Status::ERROR;
    }

    return m_status = Status::RUNNING;
}

Status LightSensor::run() {
    return m_status;
}

Status LightSensor::stop() {
    m_status = Status::STOPPING;
    m_conf.shutdown = 1;
    auto err        = std::make_error_code(veml7700_set_config(&m_dev, &m_conf));
    if (err) {
        ESP_LOGE(TAG, "Failed to set light sensor config: %s", err.message().c_str());
        return Status::ERROR;
    }
    return m_status = Status::STOPPED;
}

std::expected<uint32_t, std::error_code> LightSensor::readLightLevel() {
    uint32_t value_lux;
    if (m_status != Status::RUNNING) {
        ESP_LOGE(TAG, "Light sensor is not running, illegal operation");
        m_err = ESP_ERR_NOT_ALLOWED;
        return std::unexpected(std::make_error_code(m_err));
    }

    auto err = veml7700_get_ambient_light(&m_dev, &m_conf, &value_lux);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ambient light value: %s", esp_err_to_name(err));
        m_err = err;
        m_status = Status::ERROR;
        return std::unexpected(std::make_error_code(err));
    }

    return value_lux;
}
