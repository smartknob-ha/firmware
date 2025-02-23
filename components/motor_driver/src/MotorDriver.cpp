#include "MotorDriver.hpp"

using Status = sdk::Component::Status;
using res = sdk::Component::res;

Status MotorDriver::initialize() {
    if (Status::RUNNING <= m_status && m_status >= Status::STOPPING) {
        return m_status;
    }

    m_status = Status::INITIALIZING;

    m_motor->initialize();
    m_motor->enable();

    m_haptics->update_detent_config(espp::detail::COARSE_VALUES_STRONG_DETENTS);
    m_haptics->start();

    return m_status = Status::RUNNING;
}

Status MotorDriver::run() {
    if (m_status == Status::RUNNING) {
        m_motor->loop_foc();
    }
    return m_status;
}

Status MotorDriver::stop() {
    m_haptics->stop();
    m_motor->disable();

	return m_status = Status::STOPPED;
}

void MotorDriver::setDetentConfig(const espp::detail::DetentConfig& config, const int position) const {
    m_haptics->update_detent_config(config);
    m_haptics->set_position(position);
}

void MotorDriver::setSensor(const std::shared_ptr<encoder>& magnetic_encoder) {
    m_encoder = magnetic_encoder;
    m_driver  = std::make_shared<espp::BldcDriver>(m_driverConfig);

    m_motorConfig.sensor = magnetic_encoder;
    m_motorConfig.driver = m_driver;
    m_motor              = std::make_shared<bldcMotor>(m_motorConfig);

    m_hapticsConfig.motor = *m_motor.get();
    m_haptics = std::make_unique<BldcHaptics>(m_hapticsConfig);
}

void MotorDriver::setZero() const {
    m_haptics->stop();

    updateMotionControlType(espp::detail::MotionControlType::ANGLE);

    constexpr float dampener = M_PI / 72;

    auto angle = m_motor->get_shaft_angle();

    ESP_LOGI(TAG, "shaft angle: %f", angle);

    while (angle < 0.0) {
        TickType_t start = xTaskGetTickCount();
        m_motor->move(angle + dampener);
        m_motor->loop_foc();
        xTaskDelayUntil(&start, portTICK_PERIOD_MS);
        angle = m_motor->get_shaft_angle();
    }

    while (angle > 0.0) {
        TickType_t start = xTaskGetTickCount();
        m_motor->move(angle - dampener);
        m_motor->loop_foc();
        xTaskDelayUntil(&start, portTICK_PERIOD_MS);
        angle = m_motor->get_shaft_angle();
    }

    ESP_LOGI(TAG, "settled angle: %f", angle);

    updateMotionControlType(espp::detail::MotionControlType::TORQUE);
    setDetentConfig(espp::detail::COARSE_VALUES_STRONG_DETENTS, 50);
    m_haptics->start();
}

void MotorDriver::updateMotionControlType(espp::detail::MotionControlType motionControlType) const {
    m_motor->disable();
    m_motor->set_motion_control_type(motionControlType);
    m_motor->enable();
}
