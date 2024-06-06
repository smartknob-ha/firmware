#ifndef FIRMWARE_COMPONENTS_MOTOR_DRIVER_INCLUDE_MOTORDRIVER_HPP
#define FIRMWARE_COMPONENTS_MOTOR_DRIVER_INCLUDE_MOTORDRIVER_HPP

#include "bldc_haptics.hpp"
#include "bldc_driver.hpp"
#include "bldc_motor.hpp"
#include "Component.hpp"
#include "MagneticEncoder.hpp"

using encoder = Mt6701_spi;
using bldcMotor = espp::BldcMotor<espp::BldcDriver, encoder>;
using BldcHaptics = espp::BldcHaptics<bldcMotor>;

class MotorDriver final : public sdk::Component {
public:
	explicit MotorDriver(std::shared_ptr<encoder> magnetic_encoder) :
		m_encoder(magnetic_encoder),
		m_driver(std::make_shared<espp::BldcDriver>(m_driverConfig)) {

		m_motorConfig.sensor = magnetic_encoder;
        m_motorConfig.driver = m_driver;
		m_motor = std::make_shared<bldcMotor>(m_motorConfig);

        BldcHaptics::Config hapticsConfig {
                .motor = *m_motor.get(),
                .kp_factor = 2,
                .kd_factor_min = 0.01,
                .kd_factor_max = 0.04,
                .log_level = espp::Logger::Verbosity::NONE
        };
		m_haptics = std::make_unique<BldcHaptics>(hapticsConfig);
	};

	/* Component override functions */
	etl::string<50> getTag() override { return TAG; };

	Status initialize() override;
	Status run() override;
	Status stop() override;
private:
	static const inline char TAG[] = "Magnetic encoder";

	std::shared_ptr<encoder> m_encoder;
	std::shared_ptr<espp::BldcDriver> m_driver;
	std::shared_ptr<bldcMotor> m_motor;
	std::unique_ptr<BldcHaptics> m_haptics;

	constexpr static espp::BldcDriver::Config m_driverConfig{
		.gpio_a_h = 9,
		.gpio_a_l = 12,
		.gpio_b_h = 10,
		.gpio_b_l = 14,
		.gpio_c_h = 11,
		.gpio_c_l = 13,
		.gpio_enable = 21,
		.gpio_fault = 44,
		.power_supply_voltage = 5.0f,
		.limit_voltage = 5.0f,
		.log_level = espp::Logger::Verbosity::NONE
	};

	bldcMotor::Config m_motorConfig {
		// measured by setting it into ANGLE_OPENLOOP and then counting how many
		// spots you feel when rotating it.
		.num_pole_pairs = 7,
		.phase_resistance =
			 5.0f, // tested by running velocity_openloop and seeing if the veloicty is ~correct
		.kv_rating =
			 320, // tested by running velocity_openloop and seeing if the velocity is ~correct
		.current_limit = 1.0f,        // Amps
		.zero_electric_offset = 0.0f, // set to zero to always calibrate, since this is a test
		.sensor_direction =
			 espp::detail::SensorDirection::UNKNOWN, // set to unknown to always calibrate, since
													// this is a test
		.foc_type = espp::detail::FocType::SPACE_VECTOR_PWM,
		.driver = {},
        .sensor = {},
        .run_sensor_update = true,
        .velocity_pid_config =
        {
                 .kp = 0.010f,
                 .ki = 1.000f,
                 .kd = 0.000f,
                 .integrator_min = -1.0f, // same scale as output_min (so same scale as current)
                 .integrator_max = 1.0f,  // same scale as output_max (so same scale as current)
                 .output_min = -1.0, // velocity pid works on current (if we have phase resistance)
                 .output_max = 1.0,  // velocity pid works on current (if we have phase resistance)
         },
        .angle_pid_config =
        {
                 .kp = 7.000f,
                 .ki = 0.300f,
                 .kd = 0.010f,
                 .integrator_min = -10.0f, // same scale as output_min (so same scale as velocity)
                 .integrator_max = 10.0f,  // same scale as output_max (so same scale as velocity)
                 .output_min = -20.0,      // angle pid works on velocity (rad/s)
                 .output_max = 20.0,       // angle pid works on velocity (rad/s)
         },
        .auto_init = false,
        .log_level = espp::Logger::Verbosity::NONE
    };

	BldcHaptics::Config m_hapticsConfig {
		.motor = *m_motor.get(),
		.kp_factor = 2,
		.kd_factor_min = 0.01,
		.kd_factor_max = 0.04,
		.log_level = espp::Logger::Verbosity::NONE
	};
};

#endif //FIRMWARE_COMPONENTS_MOTOR_DRIVER_INCLUDE_MOTORDRIVER_HPP
