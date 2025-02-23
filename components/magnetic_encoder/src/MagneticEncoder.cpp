#include "../include/MagneticEncoder.hpp"
#include "esp_system_error.hpp"

using Status = sdk::Component::Status;
using res    = sdk::Component::res;

Status MagneticEncoder::initialize() {
    // Initialize the SPI bus
    const auto spi_num = static_cast<spi_host_device_t>(CONFIG_MAGNETIC_ENCODER_SPI_BUS);
    esp_err_t  ret     = spi_bus_initialize(spi_num, &m_spiBusCfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    // Attach the encoder to the SPI bus
    ret = spi_bus_add_device(spi_num, &m_spiDevCfg, &m_spiDev);
    ESP_ERROR_CHECK(ret);

    m_dev->set_log_verbosity(espp::Logger::Verbosity::NONE);
    std::error_code err;
    m_dev->initialize(false, err);

    if (err) {
        m_err = ESP_FAIL;
        return m_status = Status::ERROR;
    }
    return m_status = Status::RUNNING;
}

Status MagneticEncoder::run() {
    std::error_code err;
    m_dev->update(err);
    if (err) {
        m_err = ESP_FAIL;
        return m_status = Status::ERROR;
    }

    return m_status = Status::RUNNING;
}

Status MagneticEncoder::stop() {
    auto err = spi_bus_remove_device(m_spiDev);
    if (err != ESP_OK) {
        m_err = err;
        return m_status = Status::ERROR;
    }

    err = spi_bus_free(static_cast<spi_host_device_t>(CONFIG_MAGNETIC_ENCODER_SPI_BUS));
    if (err != ESP_OK) {
        m_err = err;
        return m_status = Status::ERROR;
    }

    return m_status = Status::STOPPED;
}

bool MagneticEncoder::read(uint8_t* data, size_t len) const {
    // we can use the SPI_TRANS_USE_RXDATA since our length is <= 4 bytes (32
    // bits), this means we can directly use the tarnsaction's rx_data field
    static constexpr uint8_t SPIBUS_READ = 0x80;
    spi_transaction_t        t           = {
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
        t.flags     = SPI_TRANS_USE_RXDATA;
        t.rx_buffer = nullptr;
    }
    esp_err_t err = spi_device_transmit(m_spiDev, &t);
    if (err != ESP_OK) { return false; }
    if (len <= 4) {
        // copy the data from the rx_data field
        for (size_t i = 0; i < len; i++) { data[i] = t.rx_data[i]; }
    }
    return true;
}

std::expected<double, std::error_code> MagneticEncoder::getDegrees() const {
    if (m_status < Status::RUNNING) { return std::unexpected(std::make_error_code(ESP_ERR_INVALID_STATE)); }
    return m_dev->get_degrees() * -1.0l;
}

std::expected<double, std::error_code> MagneticEncoder::getRadians() const {
    if (m_status < Status::RUNNING) { return std::unexpected(std::make_error_code(ESP_ERR_INVALID_STATE)); }
    return m_dev->get_radians();
}

std::expected<std::shared_ptr<Mt6701_spi>, std::error_code> MagneticEncoder::getDevice() {
    if (m_status < Status::RUNNING) { return std::unexpected(std::make_error_code(ESP_ERR_INVALID_STATE)); }

    return m_dev;
}