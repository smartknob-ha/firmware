#include <veml7700.h>

#include <cmath>

#include "../include/LightSensor.hpp"
#include "esp_err.h"
#include "esp_log.h"

#define SDA_GPIO_NUM CONFIG_LIGHT_SENSOR_SDA_GPIO_NUM
#define SCL_GPIO_NUM CONFIG_LIGHT_SENSOR_SCL_GPIO_NUM


sdk::res LightSensor::getStatus() { return Ok(m_status); }

sdk::res LightSensor::initialize() {
    esp_err_t err;
    m_status = sdk::ComponentStatus::INITIALIZING;
    ESP_LOGI(TAG, "Setting up veml7700 light sensor component");

    memset(&m_dev, 0, sizeof(i2c_dev_t));
    err = i2cdev_init();
    if (err) { RETURN_ON_ERR_MSG(err, "Failed to init i2c device: "); }

    err = veml7700_init_desc(&m_dev, I2C_NUM_0, static_cast<gpio_num_t>(SDA_GPIO_NUM), static_cast<gpio_num_t>(SCL_GPIO_NUM));
    if (err) { RETURN_ON_ERR_MSG(err, "Failed to init light sensor i2c communication: "); }

    err = veml7700_probe(&m_dev);
    if (err) { RETURN_ON_ERR_MSG(err, "Failed to probe light sensor: "); }

    m_conf.shutdown = 0;
    err             = veml7700_set_config(&m_dev, &m_conf);
    if (err) { RETURN_ON_ERR_MSG(err, "Failed to set light sensor config: "); }

    m_status = sdk::ComponentStatus::RUNNING;
    return Ok(sdk::ComponentStatus::INITIALIZING);
}

sdk::res LightSensor::stop() {
    m_conf.shutdown = 1;
    esp_err_t err   = veml7700_set_config(&m_dev, &m_conf);
    if (err) { RETURN_ON_ERR_MSG(err, "Failed to set light sensor config: "); }
    m_status = sdk::ComponentStatus::DEINITIALIZED;
    return Ok(sdk::ComponentStatus::STOPPED);
}

Result<uint32_t, etl::string<128>> LightSensor::readLightLevel() {
    uint32_t value_lux;
    if (m_status != sdk::ComponentStatus::RUNNING) { return Err(etl::string<128>("not initialized")); }

    esp_err_t err = veml7700_get_ambient_light(&m_dev, &m_conf, &value_lux);
    if (err) { RETURN_ON_ERR_MSG(err, "Failed to read ambient light value: "); }

    return Ok(value_lux);
}
