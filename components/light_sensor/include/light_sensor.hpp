#ifndef LIGHT_SENSOR_HPP
#define LIGHT_SENSOR_HPP

#include <i2cdev.h>
#include <veml7700.h>

#include "component.hpp"

namespace light_sensor {

class light_sensor : public component {
public:
	light_sensor() {};
	~light_sensor() = default;

	/* Component override functions */
	etl::string<50> get_tag() override { return TAG; };
	res get_status() override;
	res initialize() override;
	res stop() override;

    Result<uint32_t, etl::string<128>> read_light_level();
private:
	static const inline char TAG[] = "Light sensor";

	COMPONENT_STATUS m_status = UNINITIALIZED;
	etl::string<128> m_err_status;

    i2c_dev_t m_dev;
    veml7700_config_t m_conf = {
        .gain = VEML7700_GAIN_DIV_8,
        .integration_time = VEML7700_INTEGRATION_TIME_100MS,
        .persistence_protect = VEML7700_PERSISTENCE_PROTECTION_4,
        .interrupt_enable = 0,
        .shutdown = 0,
        .threshold_high = 0,
        .threshold_low = 0,
        .power_saving_mode = VEML7700_POWER_SAVING_MODE_500MS,
        .power_saving_enable = 1,


    };
};

} /* namespace light_sensor */


#endif /* LIGHT_SENSOR_HPP */