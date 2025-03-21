#ifndef strain_sensor_HPP
#define strain_sensor_HPP

#include <hx711.h>

#include "simple_lowpass_filter.hpp"

#include "Component.hpp"
#include "ConfigProvider.hpp"

/**
 * @note hx711 datasheet: https://cdn.sparkfun.com/datasheets/Sensors/ForceFlex/hx711_english.pdf
 */

class StrainSensor final : public sdk::Component {
public:
    enum StrainLevel {
        RESTING = 1,
        LIGHT_PRESS,
        HARD_PRESS,
        MAX
    };

    class Config final : public sdk::ConfigObject<4, 256, "Strain sensor"> {
        using Base = ConfigObject;

    public:
        sdk::ConfigField<int32_t> restingValue{INT32_MAX, "restingValue"};
        sdk::ConfigField<int32_t> lightPressOffsetValue{INT32_MAX, "lightPressOffsetValue"};
        sdk::ConfigField<int32_t> hardPressOffsetValue{INT32_MAX, "hardPressOffsetValue"};
        sdk::ConfigField<int32_t> strainNoiseValue{INT32_MAX, "strainNoiseValue"};

        void allocateFields() {
            restingValue     = allocate(restingValue);
            lightPressOffsetValue = allocate(lightPressOffsetValue);
            hardPressOffsetValue  = allocate(hardPressOffsetValue);
            strainNoiseValue = allocate(strainNoiseValue);
        }

        explicit Config(const nlohmann::json& data) : Base(data) {
            allocateFields();
        }

        Config() {
            allocateFields();
        }

        int32_t getStrainValue(StrainLevel level) const;

        /**
         * @brief Access config value
         * @param level
         * @param value
         */
        void updateField(StrainLevel level, int32_t value);

        void updateField(const sdk::ConfigField<int32_t>& field, const int32_t& newValue) { Base::updateField(field, newValue); }
    };

    StrainSensor() :
        m_hx711_dev({
            .dout   = static_cast<gpio_num_t>(CONFIG_STRAIN_SENSOR_DOUT_GPIO_NUM),
            .pd_sck = static_cast<gpio_num_t>(CONFIG_STRAIN_SENSOR_SCK_GPIO_NUM),
            .gain   = HX711_GAIN_A_64,
        }
    ){};

    enum CalibrationState {
        INACTIVE,
        WAITING_FOR_THRESHOLD_PASS,
        ACTIVE,
        FINISHED,
        ERROR
    };

    struct StrainState {
        StrainLevel level;
        uint8_t     percentage;
    };

    static inline const etl::array<etl::string<15>, static_cast<uint8_t>(StrainLevel::MAX)> LevelToString{
    "",
            "resting",
            "light press",
            "hard press"};

    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };
    Status          getStatus() override;
    Status          initialize() override;
    Status          stop() override;
    Status          run() override;

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
    std::expected<int32_t, std::error_code> readStrainLevel();

    /**
     * @brief Blocking function that waits for the sensor to become ready and read
     * @param samples Number of times to read from the sensor for the average
     * @return int32_t, esp_err_t on error
     */
    std::expected<int32_t, std::error_code> readAverageStrainLevel(size_t samples);

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
    static constexpr char TAG[] = "Strain sensor";

    Status           m_status            = Status::UNINITIALIZED;
    CalibrationState m_calibrationStatus = CalibrationState::INACTIVE;
    std::error_code  m_calibrationError  = std::make_error_code(ESP_OK);
    Config           m_config;
    int32_t          m_restingStateNoise = 0;
    float            m_filteredRestingLevel = 0;

    espp::SimpleLowpassFilter m_restingFilter;

    hx711_t m_hx711_dev;
};

#endif /* strain_sensor_HPP */