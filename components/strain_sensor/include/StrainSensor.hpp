#ifndef strain_sensor_HPP
#define strain_sensor_HPP

#include <hx711.h>

#include "Component.hpp"

class StrainSensor : public sdk::Component {
public:
    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };
    Status          getStatus() override;
    Status          initialize() override;
    Status          stop() override;
    Status run() override { return m_status; };

    std::expected<int32_t, std::error_code> readStrainLevel();

private:
    static const inline char TAG[] = "Light sensor";

    Status m_status = Status::UNINITIALIZED;
    etl::string<128> m_err_status;

    hx711_t m_hx711_dev = {
            .dout   = static_cast<gpio_num_t>(CONFIG_STRAIN_SENSOR_DOUT_GPIO_NUM),
            .pd_sck = static_cast<gpio_num_t>(CONFIG_STRAIN_SENSOR_SCK_GPIO_NUM),
            .gain   = HX711_GAIN_A_64,
    };
};

#endif /* strain_sensor_HPP */