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
    enum StrainLevel {
        RESTING,
        LIGHT_PRESS,
        HARD_PRESS,
        MAX
    };

    class Config final : public sdk::ConfigObject<4, 512, "Strain sensor"> {
        using Base = sdk::ConfigObject<4, 512, "Strain sensor">;

    public:
        sdk::ConfigField<uint32_t> restingValue{UINT32_MAX, "restingValue"};
        sdk::ConfigField<uint32_t> lightPressValue{UINT32_MAX, "lightPressValue"};
        sdk::ConfigField<uint32_t> hardPressValue{UINT32_MAX, "hardPressValue"};
        sdk::ConfigField<uint32_t> strainNoiseValue{UINT32_MAX, "strainNoiseValue"};

        void allocateFields() {
            restingValue     = allocate(restingValue);
            lightPressValue  = allocate(lightPressValue);
            hardPressValue   = allocate(hardPressValue);
            strainNoiseValue = allocate(strainNoiseValue);
        }

        explicit Config(const nlohmann::json& data) : Base(data) {
            allocateFields();
        }

        Config() : Base() {
            allocateFields();
        }

        uint32_t getStrainValue(StrainLevel level);

        /**
         * @brief Access config value
         * @param level
         * @param value
         */
        void updateField(StrainLevel level, uint32_t value);

        void updateField(const sdk::ConfigField<uint32_t>& field, const uint32_t& newValue) { Base::updateField(field, newValue); }
    };

    enum CalibrationState {
        INACTIVE,
        WAITING_FOR_TRESHOLD_PASS,
        ACTIVE,
        FINISHED,
        ERROR
    };

    struct StrainState {
        StrainLevel level;
        uint8_t     percentage;
    };

    static inline const etl::array<etl::string<15>, (uint8_t) StrainLevel::MAX> LevelToString{
            "resting",
            "light press",
            "hard press"};

    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };
    Status          getStatus() override;
    Status          initialize() override;
    Status          stop() override;
    Status          run() override { return m_status; };

    /**
     * @brief Whether setup has been completed in the pass
     * @return True when no settings are saved
     */
    bool needsFirstTimeSetup() { return m_config.isDefault(); }

    /**
     * @brief Checks current strain level against those saved in configuration
     * @return pressState, esp_err_t on error
     */
    std::expected<StrainState, std::error_code> getPressState();

    /**
     * @brief Blocking function that waits for the sensor to become ready and read
     * @return int32_t, esp_err_t on error
     */
    std::expected<uint32_t, std::error_code> readStrainLevel();

    /**
     * @brief Blocking function that waits for the sensor to become ready and read
     * @param samples Number of times to read from the sensor for the average
     * @return uint32_t, esp_err_t on error
     */
    std::expected<uint32_t, std::error_code> readAverageStrainLevel(size_t samples);

    /**
     * @brief Saves config to flash
     * @return esp_err_t on error
     */
    std::error_code saveConfig();

    /**
     * @brief Resets entire configuration to factory defaults
     * @return esp_err_t on error
     */
    std::error_code resetConfig();

    /**
     * @brief Calibrate configuration settings for noise value of strain sensor
     * @param save Whether to immediately save new value to flash
     * @return esp_err_t on error
     */
    std::error_code calibrateNoiseValue(bool save = false);

    /**
     * @brief Returns calibration state when calibrating
     * @return
     */
    std::expected<CalibrationState, std::error_code> getCalibrationState();

    void calibrateValue(StrainSensor::StrainLevel strainLevel, bool save = false);

private:
    static const inline char TAG[] = "Strain sensor";

    Status           m_status            = Status::UNINITIALIZED;
    CalibrationState m_calibrationStatus = CalibrationState::INACTIVE;
    std::error_code  m_calibrationError  = std::make_error_code(ESP_OK);
    Config           m_config;
    uint32_t         m_restingStateNoise = 0;

    hx711_t m_hx711_dev = {
            .dout   = static_cast<gpio_num_t>(CONFIG_STRAIN_SENSOR_DOUT_GPIO_NUM),
            .pd_sck = static_cast<gpio_num_t>(CONFIG_STRAIN_SENSOR_SCK_GPIO_NUM),
            .gain   = HX711_GAIN_A_128,
    };


};

#endif /* strain_sensor_HPP */