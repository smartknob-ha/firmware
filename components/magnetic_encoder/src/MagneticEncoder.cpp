#include "../include/MagneticEncoder.hpp"

sdk::res MagneticEncoder::getStatus() {
	return m_status;
}
sdk::res MagneticEncoder::initialize() {
	esp_err_t ret;
	// Initialize the SPI bus
	auto spi_num = static_cast<spi_host_device_t>(CONFIG_MAGNETIC_ENCODER_SPI_BUS);
	ret = spi_bus_initialize(spi_num, &m_spi_buscfg, SPI_DMA_CH_AUTO);
	ESP_ERROR_CHECK(ret);
	// Attach the encoder to the SPI bus
	ret = spi_bus_add_device(spi_num, &m_spi_devcfg, &m_spi_dev);
	ESP_ERROR_CHECK(ret);

	m_dev.set_log_verbosity(espp::Logger::Verbosity::NONE);
	std::error_code err;
	m_dev.initialize(false, err);

	if (err) {
		return std::unexpected(err);
	}

	m_status = sdk::ComponentStatus::RUNNING;
	return m_status;
}

sdk::res MagneticEncoder::run() {
	std::error_code err;
	m_dev.update(err);
	if (err) {
		return std::unexpected(err);
	}

	return sdk::ComponentStatus::RUNNING;
}

sdk::res MagneticEncoder::stop() {
	return sdk::ComponentStatus::RUNNING;
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
		.rx_buffer = nullptr,
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

std::expected<double, std::error_code> MagneticEncoder::get_degrees() {
	ESP_LOGD(TAG, "get_degrees");
	if (m_status < sdk::ComponentStatus::RUNNING) {
		return std::unexpected(std::make_error_code(std::errc::address_not_available));
	}
	return m_dev.get_degrees() * - 1.0l;
}

std::expected<double, std::error_code> MagneticEncoder::get_radians() {
	ESP_LOGD(TAG, "get_radians");
	if (m_status < sdk::ComponentStatus::RUNNING) {
		return std::unexpected(std::make_error_code(std::errc::address_not_available));
	}
	return m_dev.get_radians();
}
