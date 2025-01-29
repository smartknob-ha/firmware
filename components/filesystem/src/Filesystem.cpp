#include "Filesystem.hpp"

#include <Component.hpp>
#include <esp_flash.h>
#include <etl/string_stream.h>
#include <fcntl.h>

#include "HttpServer.hpp"
#include "esp_http_server.h"
#include "esp_littlefs.h"
#include "esp_ota_ops.h"
#include "mbedtls/sha256.h"

using Status = sdk::Component::Status;
using res    = sdk::Component::res;

Status Filesystem::initialize() {
    if (partitionsAreMounted) {
        ESP_LOGI(TAG, "Partitions are already mounted");
        if (const auto err = registerOTA()) {
            ESP_LOGE(TAG, "Failed to register OTA handler: %s", err.message().c_str());
            return m_status = Status::ERROR;
        }
    }

    esp_vfs_littlefs_conf_t mainPartitionConf = {
            .base_path              = MAIN_PARTITION_MOUNT_POINT,
            .partition_label        = nullptr,
            .partition              = nullptr,
            .format_if_mount_failed = false,
            .read_only              = false,
            .dont_mount             = false,
            .grow_on_mount          = true};

    constexpr esp_vfs_littlefs_conf_t staticAssetsPartitionConf = {
            .base_path              = STATIC_PARTITION_MOUNT_POINT,
            .partition_label        = STATIC_PARTITION_LABEL,
            .partition              = nullptr,
            .format_if_mount_failed = false,
            .read_only              = false,
            .dont_mount             = false,
            .grow_on_mount          = true};

    // Determine which partition to mount
    if (m_currentBootPartitionLabel = esp_ota_get_running_partition()->label; m_currentBootPartitionLabel == OTA_A_LABEL) {
        ESP_LOGI(TAG, "Currently running from OTA-A partition");
        mainPartitionConf.partition_label = OTA_A_MAIN_PARTITION_LABEL;
        m_currentAssetsLabel              = OTA_A_MAIN_PARTITION_LABEL;
    } else if (m_currentBootPartitionLabel == OTA_B_LABEL) {
        ESP_LOGI(TAG, "Currently running from OTA-B partition");
        mainPartitionConf.partition_label = OTA_B_MAIN_PARTITION_LABEL;
        m_currentAssetsLabel              = OTA_B_MAIN_PARTITION_LABEL;
    } else {
        ESP_LOGE(TAG, "Unknown partition label: %s", m_currentBootPartitionLabel.c_str());
        return m_status = Status::ERROR;
    }

    if (const esp_err_t err = esp_vfs_littlefs_register(&mainPartitionConf); err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount Main LittleFS partition at %s: %s", mainPartitionConf.base_path, esp_err_to_name(err));
        return m_status = Status::ERROR;
    }

    if (const esp_err_t err = esp_vfs_littlefs_register(&staticAssetsPartitionConf); err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount Static Assets LittleFS partition at %s: %s", staticAssetsPartitionConf.base_path, esp_err_to_name(err));
        return m_status = Status::ERROR;
    }

    // Make sure the partitions are not remounted when the component is restarted
    partitionsAreMounted = true;

    if (const auto err = registerOTA()) {
        ESP_LOGE(TAG, "Failed to register OTA handler: %s", err.message().c_str());
        return m_status = Status::ERROR;
    }

    return m_status = Status::RUNNING;
}

std::expected<Filesystem::PartitionInfo, std::error_code> Filesystem::partitionInfo(const etl::string<20>& partitionLabel) {
    std::size_t totalBytes = 0;
    std::size_t usedBytes  = 0;
    if (const auto ret = esp_littlefs_info(partitionLabel.c_str(), &totalBytes, &usedBytes); ret != ESP_OK) {
        return std::unexpected(std::make_error_code(ret));
    }

    return PartitionInfo{totalBytes, usedBytes};
}

std::expected<Filesystem::PartitionInfo, std::error_code> Filesystem::otaAssetsInfo() {
    return partitionInfo(m_currentAssetsLabel);
}

std::expected<Filesystem::PartitionInfo, std::error_code> Filesystem::staticAssetsInfo() {
    return partitionInfo(STATIC_PARTITION_LABEL);
}


Status Filesystem::run() {
    return m_status = Status::RUNNING;
}

Status Filesystem::stop() {
    // Don't unmount the partition as there is no reason to do so
    return m_status = Status::STOPPED;
}

Status Filesystem::getStatus() {
    return m_status;
}

esp_err_t Filesystem::otaHandler(httpd_req_t* req) {
    ESP_LOGD(TAG, "Received OTA request");

    esp_ota_handle_t       otaHandle    = 0;
    const esp_partition_t* otaPartition = esp_ota_get_next_update_partition(nullptr);

    // Determine which partitions to write to
    const auto             otaAssetsPartitionLabel = m_currentAssetsLabel == OTA_A_MAIN_PARTITION_LABEL ? OTA_B_MAIN_PARTITION_LABEL : OTA_A_MAIN_PARTITION_LABEL;
    const esp_partition_t* otaAssetsPartition      = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, otaAssetsPartitionLabel);

    constexpr auto               headerSize = sizeof(OtaInfoHeader);
    std::array<char, headerSize> header;

    if (req->content_len < headerSize) {
        handleOtaError(req, "OTA request has invalid data, too small", HTTPD_400_BAD_REQUEST);
        return ESP_FAIL;
    }

    auto err = receiveBuffer(req, header, headerSize);
    if (err != ESP_OK) {
        return handleOtaError(req, err, "Failed to receive OTA header", HTTPD_500_INTERNAL_SERVER_ERROR);
    }

    OtaInfoHeader& otaInfo = *reinterpret_cast<OtaInfoHeader*>(header.data());

    if (!verifyVersionNumber(otaInfo.firmwareVersion)) {
        handleOtaError(req, "OTA request has invalid version number", HTTPD_400_BAD_REQUEST);
        return ESP_FAIL;
    }

    if (otaInfo.appSize > otaPartition->size) {
        handleOtaError(req, "App size too large", HTTPD_400_BAD_REQUEST);
        return ESP_FAIL;
    }

    if (otaInfo.otaAssetsSize > otaAssetsPartition->size) {
        handleOtaError(req, "OTA assets size too large", HTTPD_400_BAD_REQUEST);
        return ESP_FAIL;
    }

    const size_t calculatedBinarySize = headerSize + otaInfo.appSize + otaInfo.otaAssetsSize + otaInfo.staticAssetsSize;
    if (req->content_len != calculatedBinarySize) {
        handleOtaError(req, "OTA request has invalid data, total size mismatch", HTTPD_400_BAD_REQUEST);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Starting OTA process for version %s", otaInfo.firmwareVersion);
    err = esp_ota_begin(otaPartition, otaInfo.appSize, &otaHandle);
    if (err != ESP_OK) {
        return handleOtaError(req, err, "Failed to start OTA process", HTTPD_500_INTERNAL_SERVER_ERROR);
    }

    // Single receive buffer to be used for all partitions to prevent memory fragmentation
    std::array<char, CONFIG_FILESYSTEM_OTA_RECEIVE_BUFFER_SIZE> buffer;

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 for SHA-256, 1 for SHA-224

    size_t remaining = otaInfo.appSize;
    while (remaining > 0) {
        const auto dataAmount = std::min(remaining, buffer.size());
        err                   = receiveBuffer(req, buffer, dataAmount);
        if (err != ESP_OK) {
            esp_ota_abort(otaHandle);
            return handleOtaError(req, err, "Failed to receive OTA data", HTTPD_500_INTERNAL_SERVER_ERROR);
        }

        err = esp_ota_write(otaHandle, buffer.data(), dataAmount);
        if (err != ESP_OK) {
            esp_ota_abort(otaHandle);
            return handleOtaError(req, err, "Failed to write OTA data", HTTPD_500_INTERNAL_SERVER_ERROR);
        }
        mbedtls_sha256_update(&ctx, std::bit_cast<const unsigned char*>(buffer.data()), dataAmount);
        remaining -= dataAmount;
    }

    err = esp_ota_end(otaHandle);
    if (err != ESP_OK) {
        return handleOtaError(req, err, "Failed to end OTA process", HTTPD_500_INTERNAL_SERVER_ERROR);
    }

    ESP_LOGD(TAG, "Finished updating app partition, checking integrity");

    std::array<uint8_t, 32> appSha;
    mbedtls_sha256_finish(&ctx, std::bit_cast<unsigned char*>(appSha.data()));

    if (otaInfo.appSha != appSha) {
        handleOtaError(req, "OTA app partition integrity check failed, SHA256 does not match", HTTPD_500_INTERNAL_SERVER_ERROR);
        ESP_LOGE(TAG, "Calculated SHA256: %s", sha256ToString(appSha).c_str());
        ESP_LOGE(TAG, "Incoming SHA256:   %s", sha256ToString(otaInfo.appSha).c_str());
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Integrity verified, updating OTA assets partition");

    err = otaWritePartition(req, otaAssetsPartition, otaInfo.otaAssetsSize, otaInfo.otaAssetsSha, buffer);
    if (err != ESP_OK) {
        // Logging is handled in the otaWritePartition function
        return err;
    }

    ESP_LOGD(TAG, "Finished updating OTA assets");

    if (otaInfo.hasStaticAssets) {
        ESP_LOGD(TAG, "Update contains static assets, updating static assets partition");
        const esp_partition_t* staticAssetsPartition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, STATIC_PARTITION_LABEL);
        err                                          = otaWritePartition(req, staticAssetsPartition, otaAssetsPartition->size, otaInfo.staticAssetsSha, buffer);
        if (err != ESP_OK) {
            // Logging is handled in the otaWritePartition function
            return err;
        }
        ESP_LOGD(TAG, "Finished updating static assets");
    }

    ESP_LOGD(TAG, "Marking OTA partition as bootable");
    err = esp_ota_set_boot_partition(otaPartition);
    if (err != ESP_OK) {
        return handleOtaError(req, err, "Failed making OTA partition bootable", HTTPD_500_INTERNAL_SERVER_ERROR);
    }

    ESP_LOGI(TAG, "OTA process completed, restarting");
    xTaskCreate(restartTask, "RestartTask", CONFIG_RUN_TASK_STACK_SIZE, nullptr, 5, nullptr);
    httpd_resp_send(req, "OK, restarting", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void Filesystem::restartTask(void*) {
    // Make sure the handler has returned before restarting
    vTaskDelay(1000);
    esp_restart();
}

esp_err_t Filesystem::otaWritePartition(httpd_req_t* req, const esp_partition_t* partition, const size_t updateSize, const std::span<uint8_t> sha256, const std::span<char> buffer) {
    const auto partitionLabel = partition->label;

    ESP_LOGD(TAG, "Erasing partition %s", partitionLabel);
    auto err = esp_partition_erase_range(partition, 0, partition->size);
    if (err != ESP_OK) {
        return handleOtaError(req, err, "Failed to erase OTA assets partition", HTTPD_500_INTERNAL_SERVER_ERROR);
    }

    ESP_LOGD(TAG, "Finished erasing, writing");

    size_t remaining = updateSize;
    while (remaining > 0) {
        const auto dataAmount = std::min(remaining, buffer.size());
        err                   = receiveBuffer(req, buffer, dataAmount);
        if (err != ESP_OK) {
            esp_partition_erase_range(partition, 0, partition->size);
            return handleOtaError(req, err, "Unrecoverable error receiving data, erasing OTA assets partition", HTTPD_500_INTERNAL_SERVER_ERROR);
        }

        err = esp_partition_write(partition, partition->size - remaining, buffer.data(), dataAmount);
        if (err != ESP_OK) {
            esp_partition_erase_range(partition, 0, partition->size);
            return handleOtaError(req, err, "Failed to write OTA assets, erasing partition", HTTPD_500_INTERNAL_SERVER_ERROR);
        }

        remaining -= dataAmount;
    }
    ESP_LOGD(TAG, "Finished writing to partition, verifying integrity");

    if (!verifyEntirePartition(partition, sha256)) {
        ESP_LOGE(TAG, "OTA app partition integrity check failed, SHA256 does not match");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA app partition integrity check failed");
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Integrity verified, finished updating partition %s", partitionLabel);
    return ESP_OK;
}

esp_err_t Filesystem::receiveBuffer(httpd_req_t* req, const std::span<char> buffer, const size_t length) {
    size_t  received          = 0;
    uint8_t timeoutRetryCount = 0;
    while (received != length) {
        const auto ret = httpd_req_recv(req, buffer.data() + received, length - received);
        switch (ret) {
            case HTTPD_SOCK_ERR_TIMEOUT:
                if (timeoutRetryCount++ < CONFIG_FILESYSTEM_OTA_MAX_TIMEOUT_RETRY) {
                    ESP_LOGW(TAG, "Timeout while receiving OTA data, retrying");
                    continue;
                }
                ESP_LOGE(TAG, "Timeout while receiving buffer");
                return ESP_ERR_TIMEOUT;
            case HTTPD_SOCK_ERR_FAIL:
                ESP_LOGE(TAG, "Error while receiving buffer");
                return ESP_FAIL;
            case HTTPD_SOCK_ERR_INVALID:
                // Shouldn't happen
                assert(1 == 0 && "Invalid arguments");
            default:
                timeoutRetryCount = 0;
                received += ret;
                break;
        }
    }
    return ESP_OK;
}

std::error_code Filesystem::registerOTA() const {
    // TODO Determine OTA path
    const auto err = m_httpServer.registerRawUri(CONFIG_FILESYSTEM_OTA_POST_PATH, HTTP_POST, otaHandler);
    if (err.value() == ESP_ERR_HTTPD_HANDLER_EXISTS) {
        ESP_LOGW(TAG, "OTA handler already registered");
        return {};
    }

    return err;
}

bool Filesystem::verifyVersionNumber(const std::span<char> sha256) {
    auto currentVersion = semver::from_string_noexcept(esp_app_get_description()->version);
    // TODO: Implement version comparison
    return true;
}

bool Filesystem::verifyEntirePartition(const esp_partition_t* partition, const std::span<uint8_t> sha256) {
    std::array<uint8_t, 32> readSha256;

    if (const auto err = esp_partition_get_sha256(partition, readSha256.data()); err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SHA256 hash of partition %s: %s", partition->label, esp_err_to_name(err));
        return false;
    }

    if (std::memcmp(readSha256.data(), sha256.data(), readSha256.size()) != 0) {
        ESP_LOGE(TAG, "SHA256 hash of partition %s does not match", partition->label);
        ESP_LOGE(TAG, "Calculated SHA256: %s", sha256ToString(readSha256).c_str());
        ESP_LOGE(TAG, "Incoming SHA256:   %s", sha256ToString(sha256).c_str());
        return false;
    }
    return true;
}

esp_err_t Filesystem::handleOtaError(httpd_req_t* req, const esp_err_t error, const etl::string_view message, const httpd_err_code_t code) {
    etl::string<50> log{message};
    log.append(": ");
    log.append(esp_err_to_name(error));

    ESP_LOGE(TAG, "%s", log.c_str());
    httpd_resp_send_err(req, code, log.c_str());
    return error;
}

void Filesystem::handleOtaError(httpd_req_t* req, const etl::string_view message, const httpd_err_code_t code) {
    ESP_LOGE(TAG, "%s", message.data());
    httpd_resp_send_err(req, code, message.data());
}

etl::string<64> Filesystem::sha256ToString(const std::span<uint8_t> sha256) {
    etl::string<64>    string;
    etl::string_stream stream(string);
    for (const auto byte: sha256) {
        stream << etl::hex << etl::setw(2) << etl::setfill('0') << byte;
    }
    return string;
}
