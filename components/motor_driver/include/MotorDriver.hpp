#ifndef FIRMWARE_COMPONENTS_MOTOR_DRIVER_INCLUDE_MOTORDRIVER_HPP
#define FIRMWARE_COMPONENTS_MOTOR_DRIVER_INCLUDE_MOTORDRIVER_HPP

#include "Component.hpp"
#include "MagneticEncoder.hpp"
#include "bldc_driver.hpp"
#include "bldc_haptics.hpp"
#include "bldc_motor.hpp"

using encoder     = Mt6701_spi;
using bldcMotor   = espp::BldcMotor<espp::BldcDriver, encoder>;
using BldcHaptics = espp::BldcHaptics<bldcMotor>;

class MotorDriver final : public sdk::Component {
public:
    MotorDriver() = default;

    /**
    * @brief Set the magnetic encoder, needed for the haptics to know the shaft angle
    * @param magnetic_encoder Mt6701_spi encoder
    */
    void setSensor(const std::shared_ptr<encoder>& magnetic_encoder);

    /**
    * @brief Returns the motor 0 degrees, takes a while and is currently unused
    */
    void setZero() const;


    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };

    Status initialize() override;
    Status run() override;
    Status stop() override;

    /**
     * @brief Sets detent config
     * @param config Type of haptic feedback
     * @param position Position within the DetentConfig, gets
     *                 clamped to `config.min_position` and `config.max_position`
     */
    void setDetentConfig(const espp::detail::DetentConfig& config, int position) const;

    /**
    * @brief Returns haptic position within the current haptic config
    */
    float getPosition() const { return m_haptics->get_position(); };

private:
    static const inline char TAG[] = "Motor driver";

    /**
    * @brief Sets the motor control type, also disables de motor momentarily
    * @param motionControlType Desired control method for the motor
    */
    void updateMotionControlType(espp::detail::MotionControlType motionControlType) const;

    std::shared_ptr<encoder>          m_encoder;
    std::shared_ptr<espp::BldcDriver> m_driver;
    std::shared_ptr<bldcMotor>        m_motor;
    std::unique_ptr<BldcHaptics>      m_haptics;

    constexpr static espp::BldcDriver::Config m_driverConfig{
            .gpio_a_h             = 9,
            .gpio_a_l             = 12,
            .gpio_b_h             = 10,
            .gpio_b_l             = 14,
            .gpio_c_h             = 11,
            .gpio_c_l             = 13,
            .gpio_enable          = 21,
            .gpio_fault           = 44,
            .power_supply_voltage = 5.0f,
            .dead_zone_ns         = 150,
            .log_level            = espp::Logger::Verbosity::NONE,
    };

    bldcMotor::Config m_motorConfig{
            // measured by setting it into ANGLE_OPENLOOP and then counting how many
            // spots you feel when rotating it.
            .num_pole_pairs = 7,
            .phase_resistance =
                    3.65f, // tested by running velocity_openloop and seeing if the veloicty is ~correct
            .kv_rating =
                    270,                 // tested by running velocity_openloop and seeing if the velocity is ~correct
            .current_limit        = 1.5f, // Amps
            .zero_electric_offset = 0.0f, // set to zero to always calibrate, since this is a test
            .sensor_direction =
                    espp::detail::SensorDirection::UNKNOWN, // set to unknown to always calibrate
            .foc_type          = espp::detail::FocType::SPACE_VECTOR_PWM,
            .driver            = {},
            .sensor            = {},
            .run_sensor_update = true,
            .velocity_pid_config =
                    {
                            .kp             = 0.010f,
                            .ki             = 1.000f,
                            .kd             = 0.000f,
                            .integrator_min = -1.0f, // same scale as output_min (so same scale as current)
                            .integrator_max = 1.0f,  // same scale as output_max (so same scale as current)
                            .output_min     = -1.0,  // velocity pid works on current (if we have phase resistance)
                            .output_max     = 1.0,   // velocity pid works on current (if we have phase resistance)
                    },
            .angle_pid_config =
                    {
                            .kp             = 7.000f,
                            .ki             = 0.300f,
                            .kd             = 0.010f,
                            .integrator_min = -10.0f, // same scale as output_min (so same scale as velocity)
                            .integrator_max = 10.0f,  // same scale as output_max (so same scale as velocity)
                            .output_min     = -20.0,  // angle pid works on velocity (rad/s)
                            .output_max     = 20.0,   // angle pid works on velocity (rad/s)
                    },
            .auto_init = false,
            .log_level = espp::Logger::Verbosity::NONE};

    BldcHaptics::Config m_hapticsConfig{
            .motor         = *m_motor.get(),
            .kp_factor     = 2,
            .kd_factor_min = 0.01,
            .kd_factor_max = 0.04,
            .log_level     = espp::Logger::Verbosity::NONE};
};

#endif // FIRMWARE_COMPONENTS_MOTOR_DRIVER_INCLUDE_MOTORDRIVER_HPP
