//
// Created by stefan on 18-5-24.
//

#ifndef FIRMWARE_COMPONENTS_MAGNETIC_ENCODER_INCLUDE_MAGNETICENCODER_HPP
#define FIRMWARE_COMPONENTS_MAGNETIC_ENCODER_INCLUDE_MAGNETICENCODER_HPP

#include "Component.hpp"
#include "butterworth_filter.hpp"

#include <mt6701.hpp>
#include <driver/spi_master.h>

using Mt6701_spi = espp::Mt6701<espp::Mt6701Interface::SSI>;
using ButterFilter = espp::ButterworthFilter<2, espp::BiquadFilterDf2>;

class MagneticEncoder final : public sdk::Component {
public:
	MagneticEncoder() :
		m_filter({.normalized_cutoff_frequency = 2.0f * m_filter_cutoff_hz * m_encoder_update_period}),
		m_dev({
			.read = std::bind(&MagneticEncoder::read, this, std::placeholders::_1, std::placeholders::_2),
			.velocity_filter = std::bind(&MagneticEncoder::m_filter_fn, this, std::placeholders::_1),
			.auto_init = false,
			.run_task = false,
			.log_level = espp::Logger::Verbosity::INFO})
			{};
	~MagneticEncoder() = default;

	/* Component override functions */
	etl::string<50> getTag() override { return TAG; };

	sdk::res getStatus() override;
	sdk::res initialize() override;
	sdk::res run() override;
	sdk::res stop() override;

	std::expected<double, std::error_code> get_degrees();
	std::expected<double, std::error_code> get_radians();

	std::expected<std::reference_wrapper<Mt6701_spi>, std::error_code> getDevice();

private:
	static const inline char TAG[] = "Magnetic encoder";
	sdk::ComponentStatus m_status = sdk::ComponentStatus::UNINITIALIZED;

	spi_device_handle_t m_spi_dev;
	spi_bus_config_t m_spi_buscfg {
		.mosi_io_num = -1,
		.miso_io_num = CONFIG_MAGNETIC_ENCODER_SPI_MISO_GPIO,
		.sclk_io_num = CONFIG_MAGNETIC_ENCODER_SPI_CLK_GPIO,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 32
	};

	spi_device_interface_config_t m_spi_devcfg {
		.mode = 0,
		.clock_speed_hz = CONFIG_MAGNETIC_ENCODER_SPI_CLOCK_SPEED,
		.input_delay_ns = 0,
		.spics_io_num = CONFIG_MAGNETIC_ENCODER_SPI_CS_GPIO,
		.queue_size = 1
	};

	static constexpr float m_filter_cutoff_hz = 10.0f;
	static constexpr float m_encoder_update_period = 0.001f; // seconds

	ButterFilter m_filter;
	float m_filter_fn(float raw) { return m_filter.update(raw); };

	Mt6701_spi m_dev;

	bool read(uint8_t* data, size_t len);
};


#endif //FIRMWARE_COMPONENTS_MAGNETIC_ENCODER_INCLUDE_MAGNETICENCODER_HPP
