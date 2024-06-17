#include "../include/MotorDriver.hpp"
#include "high_resolution_timer.hpp"

using Status = sdk::Component::Status;
using res = sdk::Component::res;

Status MotorDriver::initialize() {
    m_status = Status::INITIALIZING;

	m_motor->initialize();
    m_motor->enable();



    m_haptics->update_detent_config(espp::detail::COARSE_VALUES_STRONG_DETENTS);
    m_haptics->start();

	return m_status = Status::RUNNING;
}

Status MotorDriver::run() {

    m_motor->loop_foc();
	return Status::RUNNING;
}

Status MotorDriver::stop() {
    m_haptics->stop();
    m_motor->disable();
    m_status = Status::ERROR;
	return m_status;
}

void MotorDriver::setSensor(const std::shared_ptr<encoder> magnetic_encoder) {
    m_encoder = magnetic_encoder;
    m_driver  = std::make_shared<espp::BldcDriver>(m_driverConfig);

    m_motorConfig.sensor = magnetic_encoder;
    m_motorConfig.driver = m_driver;
    m_motor              = std::make_shared<bldcMotor>(m_motorConfig);

    m_hapticsConfig.motor = *m_motor.get();
    m_haptics = std::make_unique<BldcHaptics>(m_hapticsConfig);
}

void MotorDriver::setDetentConfig(espp::detail::DetentConfig config) {
    // ToDo: Take a position as a parameter
    int median = (config.max_position - config.min_position) / 2;
    m_haptics->set_position(median);
    m_haptics->update_detent_config(config);
}

void MotorDriver::setZero() {
    m_haptics->stop();

    updateMotionControlType(espp::detail::MotionControlType::ANGLE);

#define DAMPENER M_PI / 72

    auto angle = m_motor->get_shaft_angle();

    ESP_LOGI(TAG, "shaft angle: %f", angle);

    while (angle < 0.0) {
        TickType_t start = xTaskGetTickCount();
        m_motor->move(angle + DAMPENER);
        m_motor->loop_foc();
        xTaskDelayUntil(&start, portTICK_PERIOD_MS);
        angle = m_motor->get_shaft_angle();
    }

    while (angle > 0.0) {
        TickType_t start = xTaskGetTickCount();
        m_motor->move(angle - DAMPENER);
        m_motor->loop_foc();
        xTaskDelayUntil(&start, portTICK_PERIOD_MS);
        angle = m_motor->get_shaft_angle();
    }

    ESP_LOGI(TAG, "settled angle: %f", angle);

    updateMotionControlType(espp::detail::MotionControlType::TORQUE);
    setDetentConfig(espp::detail::ON_OFF_STRONG_DETENTS);
    m_haptics->start();
}

void MotorDriver::updateMotionControlType(espp::detail::MotionControlType motionControlType) {
    m_motor->disable();
    m_motor->set_motion_control_type(motionControlType);
    m_motor->enable();
}
