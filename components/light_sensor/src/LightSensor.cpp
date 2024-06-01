#include "../include/LightSensor.hpp"

#include <veml7700.h>

#include <cmath>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system_error.hpp"

#define SDA_GPIO_NUM CONFIG_LIGHT_SENSOR_SDA_GPIO_NUM
#define SCL_GPIO_NUM CONFIG_LIGHT_SENSOR_SCL_GPIO_NUM


sdk::res LightSensor::getStatus() { return m_status; }

sdk::res LightSensor::initialize() {
    m_status = sdk::ComponentStatus::INITIALIZING;
    ESP_LOGI(TAG, "Setting up veml7700 light sensor component");

    memset(&m_dev, 0, sizeof(i2c_dev_t));
    auto err = std::make_error_code(i2cdev_init());
    if (err) {
        ESP_LOGE(TAG, "Failed to init i2c device: %s", err.message().c_str());
        return std::unexpected(err);
    }

    err = std::make_error_code(veml7700_init_desc(&m_dev, I2C_NUM_0, static_cast<gpio_num_t>(SDA_GPIO_NUM), static_cast<gpio_num_t>(SCL_GPIO_NUM)));
    if (err) {
        ESP_LOGE(TAG, "Failed to init light sensor i2c communication: %s", err.message().c_str());
        return std::unexpected(err);
    }

    err = std::make_error_code(veml7700_probe(&m_dev));
    if (err) {
        ESP_LOGE(TAG, "Failed to probe light sensor: %s", err.message().c_str());
        return std::unexpected(err);
    }

    m_conf.shutdown = 0;
    err             = std::make_error_code(veml7700_set_config(&m_dev, &m_conf));
    if (err) {
        ESP_LOGE(TAG, "Failed to set light sensor config: %s", err.message().c_str());
        return std::unexpected(err);
    }

    m_status = sdk::ComponentStatus::RUNNING;
    return m_status;
}

sdk::res LightSensor::stop() {
    m_conf.shutdown = 1;
    auto err        = std::make_error_code(veml7700_set_config(&m_dev, &m_conf));
    if (err) {
        ESP_LOGE(TAG, "Failed to set light sensor config: %s", err.message().c_str());
        return std::unexpected(err);
    }
    m_status = sdk::ComponentStatus::DEINITIALIZED;
    return m_status;
}

std::expected<uint32_t, std::error_code> LightSensor::readLightLevel() {
    uint32_t value_lux;
    if (m_status != sdk::ComponentStatus::RUNNING) {
        ESP_LOGE(TAG, "Light sensor is not running, illegal operation");
        return std::unexpected(std::make_error_code(ESP_ERR_NOT_ALLOWED));
    }

    auto err = std::make_error_code(veml7700_get_ambient_light(&m_dev, &m_conf, &value_lux));
    if (err) {
        ESP_LOGE(TAG, "Failed to read ambient light value: %s", err.message().c_str());
        return std::unexpected(err);
    }

    return value_lux;
}
