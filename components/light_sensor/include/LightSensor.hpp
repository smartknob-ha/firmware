#ifndef LIGHT_SENSOR_HPP
#define LIGHT_SENSOR_HPP

#include <i2cdev.h>
#include <veml7700.h>

#include "Component.hpp"
#include <expected>

using Status = sdk::Component::Status;

class LightSensor : public sdk::Component {
public:
    LightSensor(){};
    ~LightSensor() = default;

    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };

    Status getStatus() override;
    Status initialize() override;
    Status run() override;
    Status stop() override;

    std::expected<uint32_t, std::error_code> readLightLevel();

private:
    static const inline char TAG[] = "Light sensor";

    i2c_dev_t         m_dev;
    veml7700_config_t m_conf = {
            .gain                = VEML7700_GAIN_DIV_8,
            .integration_time    = VEML7700_INTEGRATION_TIME_100MS,
            .persistence_protect = VEML7700_PERSISTENCE_PROTECTION_4,
            .interrupt_enable    = 0,
            .shutdown            = 0,
            .threshold_high      = 0,
            .threshold_low       = 0,
            .power_saving_mode   = VEML7700_POWER_SAVING_MODE_500MS,
            .power_saving_enable = 1,
    };
};

#endif /* LIGHT_SENSOR_HPP */