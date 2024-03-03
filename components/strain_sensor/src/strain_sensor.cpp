#include "../include/strain_sensor.hpp"

#include <cmath>

#include "esp_err.h"
#include "esp_log.h"

namespace strain_sensor {

    res strain_sensor::get_status() { return Ok(m_status); }

    res strain_sensor::initialize() {
        esp_err_t err;
        m_status = INITIALIZING;
        ESP_LOGI(TAG, "Setting up hx711 strain sensor component");

        err = hx711_init(&m_hx711_dev);
        if (err) { RETURN_ON_ERR_MSG(err, "Failed to initalize hx711: "); }

        m_status = RUNNING;
        return Ok(INITIALIZING);
    }

    res strain_sensor::stop() {
        esp_err_t err = hx711_power_down(&m_hx711_dev, true);
        if (err) { RETURN_ON_ERR_MSG(err, "Failed to power down device: "); }
        m_status = DEINITIALIZED;
        return Ok(STOPPED);
    }

    Result<int32_t, etl::string<128>> strain_sensor::read_light_level() {
        int32_t data;
        if (m_status != RUNNING) { return Err(etl::string<128>("not initialized")); }

        hx711_wait(&m_hx711_dev, 150);

        esp_err_t err = hx711_read_data(&m_hx711_dev, &data);
        if (err) { RETURN_ON_ERR_MSG(err, "Failed to read ambient light value: "); }

        return Ok(data);
    }

}/* namespace ring_lights */
