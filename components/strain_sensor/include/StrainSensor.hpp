#ifndef strain_sensor_HPP
#define strain_sensor_HPP

#include <hx711.h>

#include "Component.hpp"
#include "ConfigProvider.hpp"

/**
 * @note hx711 datasheet: https://cdn.sparkfun.com/datasheets/Sensors/ForceFlex/hx711_english.pdf
 */

class StrainSensor : public sdk::Component {
public:
    class Config final : public sdk::ConfigObject<3, 512, "Strain sensor"> {
        using Base = sdk::ConfigObject<3, 512, "Strain sensor">;

    public:

        sdk::ConfigField<unsigned long> restingValue{429386000, "restingValue"};
        sdk::ConfigField<unsigned long> lightPressValue{429384000, "lightPressValue"};
        sdk::ConfigField<unsigned long> hardPressValue{429382000, "hardPressValue"};

        void allocateFields() {
            restingValue    = allocate(restingValue);
            lightPressValue = allocate(lightPressValue);
            hardPressValue  = allocate(hardPressValue);
        }

        explicit Config(const nlohmann::json& data) : Base(data) {
            allocateFields();
        }

        Config() : Base() {
            allocateFields();
        }
    };

    enum class pressState {
        RESTING,
        LIGHT_PRESS,
        HARD_PRESS
    };

    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };
    Status          getStatus() override;
    Status          initialize() override;
    Status          stop() override;
    Status          run() override { return m_status; };

    /**
     * @brief Checks current strain level against those saved in configuration
     * @return pressState, esp_err_t on error
     */
    std::expected<pressState, std::error_code> getPressState();

    /**
     * @brief Blocking function that waits for the sensor to become ready and read
     * @return unsigned long, esp_err_t on error
     */
    std::expected<unsigned long, std::error_code> readStrainLevel();

    /**
     * @brief Blocking function that waits for the sensor to become ready and read
     * @param samples Number of times to read from the sensor for the average
     * @return unsigned long, esp_err_t on error
     */
    std::expected<unsigned long, std::error_code> readAverageStrainLevel(size_t samples);

    /**
     * @brief Saves config to flash
     * @return esp_err_t on error
     */
    std::error_code saveConfig();

    /**
     * @brief Calibrate configuration settings for resting value of strain sensor
     * @param save Whether to immediately save new value to flash
     * @return esp_err_t on error
     */
    std::error_code calibrateRestingValue(bool save = false);

    /**
     * @brief Calibrate configuration settings for light press value of strain sensor
     * @param save Whether to immediately save new value to flash
     * @return esp_err_t on error
     */
    std::error_code calibrateLightPressValue(bool save = false);

    /**
     * @brief Calibrate configuration settings for hard press value of strain sensor
     * @param save Whether to immediately save new value to flash
     * @return esp_err_t on error
     */
    std::error_code calibrateHardPressValue(bool save = false);

private:
    static const inline char TAG[] = "Light sensor";

    Status           m_status = Status::UNINITIALIZED;
    etl::string<128> m_err_status;
    Config           m_config;

    hx711_t m_hx711_dev = {
            .dout   = static_cast<gpio_num_t>(CONFIG_STRAIN_SENSOR_DOUT_GPIO_NUM),
            .pd_sck = static_cast<gpio_num_t>(CONFIG_STRAIN_SENSOR_SCK_GPIO_NUM),
            .gain   = HX711_GAIN_A_64,
    };
};

#endif /* strain_sensor_HPP */