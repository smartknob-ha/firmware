#ifndef strain_sensor_HPP
#define strain_sensor_HPP

#include <hx711.h>

#include "component.hpp"

namespace strain_sensor {

    class strain_sensor : public component {
    public:
        strain_sensor() {};
        ~strain_sensor() = default;

        /* Component override functions */
        etl::string<50> get_tag() override { return TAG; };
        res             get_status() override;
        res             initialize() override;
        res             stop() override;

        Result<int32_t, etl::string<128>> read_light_level();

    private:
        static const inline char TAG[] = "Light sensor";

        COMPONENT_STATUS m_status = UNINITIALIZED;
        etl::string<128> m_err_status;

        hx711_t m_hx711_dev = {
            .dout = static_cast<gpio_num_t>(CONFIG_STRAIN_SENSOR_DOUT_GPIO_NUM),
            .pd_sck = static_cast<gpio_num_t>(CONFIG_STRAIN_SENSOR_SCK_GPIO_NUM),
            .gain = HX711_GAIN_A_64,
        };
    };

}/* namespace strain_sensor */


#endif /* strain_sensor_HPP */