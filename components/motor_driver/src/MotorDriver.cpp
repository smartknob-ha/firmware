#include "../include/MotorDriver.hpp"

using Status = sdk::Component::Status;
using res = sdk::Component::res;

Status MotorDriver::initialize() {
	m_motor->initialize();
    m_motor->enable();

	m_haptics->update_detent_config(espp::detail::COARSE_VALUES_STRONG_DETENTS);
    m_haptics->start();

	m_status = Status::RUNNING;
	return m_status;
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
