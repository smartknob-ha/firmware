#include "../include/MotorDriver.hpp"

using Status = sdk::Component::Status;
using res = sdk::Component::res;

Status MotorDriver::initialize() {
    ESP_LOGD(TAG, "entered initialize()");
    m_status = Status::INITIALIZING;

    ESP_LOGD(TAG, "calling motor->initialize()");
	m_motor->initialize();
    ESP_LOGD(TAG, "calling motor->enable()");
    m_motor->enable();

    ESP_LOGD(TAG, "calling haptics->update_detent_config()");
	m_haptics->update_detent_config(espp::detail::COARSE_VALUES_STRONG_DETENTS);
    ESP_LOGD(TAG, "calling haptics->start()");
    m_haptics->start();

    ESP_LOGD(TAG, "leaving initialize()");
	return m_status = Status::RUNNING;
}

Status MotorDriver::run() {
    ESP_LOGD(TAG, "entered run()");

    m_motor->loop_foc();

    ESP_LOGD(TAG, "leaving run()");
	return Status::RUNNING;
}

Status MotorDriver::stop() {
    m_haptics->stop();
    m_motor->disable();
    m_status = Status::ERROR;
	return m_status;
}
