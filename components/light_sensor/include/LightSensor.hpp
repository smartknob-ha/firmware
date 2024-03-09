#ifndef LIGHT_SENSOR_HPP
#define LIGHT_SENSOR_HPP

#include <i2cdev.h>
#include <veml7700.h>

#include "Component.hpp"

class LightSensor : public Component {
public:
    LightSensor(){};
    ~LightSensor() = default;

    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };

    res getStatus() override;
    res initialize() override;
    res stop() override;

    Result<uint32_t, etl::string<128>> readLightLevel();

private:
    static const inline char TAG[] = "Light sensor";

    ComponentStatus m_status = ComponentStatus::UNINITIALIZED;
    etl::string<128> m_errStatus;

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