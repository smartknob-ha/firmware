#include "../include/MagneticEncoder.hpp"

sdk::res MagneticEncoder::getStatus() {
	return Ok(m_status);
}
sdk::res MagneticEncoder::initialize() {
	std::error_code err;
	m_dev.initialize(false, err);

	if (err) {
		etl::string<128> err_msg;
		snprintf(err_msg.data(), 128, "Failed to initialize magnetic encoder: %s", err.message().c_str());
		err_msg.repair();
		return Err(err_msg);
	}

	m_status = sdk::ComponentStatus::RUNNING;
	return Ok(m_status);
}

sdk::res MagneticEncoder::run() {
	std::error_code err;
	m_dev.update(err);
	if (err) {
		etl::string<128> err_msg;
		snprintf(err_msg.data(), 128, "Failed to run magnetic encoder: %s", err.message().c_str());
		err_msg.repair();
		return Err(err_msg);
	}

	return Ok(sdk::ComponentStatus::RUNNING);
}

sdk::res MagneticEncoder::stop() {
	return Ok(sdk::ComponentStatus::RUNNING);
}

bool MagneticEncoder::read(uint8_t* data, size_t len) {
	// we can use the SPI_TRANS_USE_RXDATA since our length is <= 4 bytes (32
	// bits), this means we can directly use the tarnsaction's rx_data field
	static constexpr uint8_t SPIBUS_READ = 0x80;
	spi_transaction_t t = {
		.flags = 0,
		.cmd = 0,
		.addr = SPIBUS_READ,
		.length = len * 8,
		.rxlength = len * 8,
		.user = nullptr,
		.tx_buffer = nullptr,
		.rx_buffer = data,
	};
	if (len <= 4) {
		t.flags = SPI_TRANS_USE_RXDATA;
		t.rx_buffer = nullptr;
	}
	esp_err_t err = spi_device_transmit(m_spi_dev, &t);
	if (err != ESP_OK) {
		return false;
	}
	if (len <= 4) {
		// copy the data from the rx_data field
		for (size_t i = 0; i < len; i++) {
			data[i] = t.rx_data[i];
		}
	}
	return true;
}

std::optional<double> MagneticEncoder::get_degrees() {
	return std::optional<double>(m_dev.get_degrees());
}

std::optional<double> MagneticEncoder::get_radians() {
	return std::optional<double>(m_dev.get_radians());
}
