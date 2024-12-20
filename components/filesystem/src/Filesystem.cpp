#include "Filesystem.hpp"

#include <Component.hpp>

#include "esp_littlefs.h"
#include "esp_ota_ops.h"
#include <fcntl.h>

using Status = sdk::Component::Status;
using res    = sdk::Component::res;

Status Filesystem::initialize() {
    if (partitionsAreMounted) {
        ESP_LOGI(TAG, "Partitions are already mounted");
        return m_status = registerOTA();
    }

    esp_vfs_littlefs_conf_t mainPartitionConf = {
            .base_path = MAIN_PARTITION_MOUNT_POINT,
            .partition_label = nullptr,
            .partition = nullptr,
            .format_if_mount_failed = false,
            .read_only = false,
            .dont_mount = false,
            .grow_on_mount = true
    };

    constexpr esp_vfs_littlefs_conf_t staticAssetsPartitionConf = {
            .base_path = STATIC_PARTITION_MOUNT_POINT,
            .partition_label = STATIC_PARTITION_LABEL,
            .partition = nullptr,
            .format_if_mount_failed = false,
            .read_only = false,
            .dont_mount = false,
            .grow_on_mount = true
    };

    // Determine which partition to mount
    if (const auto currentPartition = esp_ota_get_running_partition()->label; strcmp(currentPartition, OTA_A_LABEL) == 0) {
        ESP_LOGI(TAG, "Currently running from OTA-A partition");
        mainPartitionConf.partition_label = OTA_A_MAIN_PARTITION_LABEL;
        otaAssetsLabel                    = OTA_A_MAIN_PARTITION_LABEL;
    } else if (strcmp(currentPartition, OTA_B_LABEL) == 0) {
        ESP_LOGI(TAG, "Currently running from OTA-B partition");
        mainPartitionConf.partition_label = OTA_B_MAIN_PARTITION_LABEL;
        otaAssetsLabel                    = OTA_B_MAIN_PARTITION_LABEL;
    } else {
        ESP_LOGE(TAG, "Unknown partition label: %s", currentPartition);
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

    return m_status = registerOTA();
}

std::expected<Filesystem::PartitionInfo, std::error_code> Filesystem::partitionInfo(const etl::string<20>& partitionLabel) {
    std::size_t totalBytes = 0;
    std::size_t usedBytes  = 0;
    if (auto ret = esp_littlefs_info(partitionLabel.c_str(), &totalBytes, &usedBytes); ret != ESP_OK) { return std::unexpected(std::make_error_code(ret)); }

    return PartitionInfo{totalBytes, usedBytes};
}

std::expected<Filesystem::PartitionInfo, std::error_code> Filesystem::otaAssetsInfo() { return partitionInfo(otaAssetsLabel); }

std::expected<Filesystem::PartitionInfo, std::error_code> Filesystem::staticAssetsInfo() { return partitionInfo(STATIC_PARTITION_LABEL); }


Status Filesystem::run() { return m_status = Status::RUNNING; }

Status Filesystem::stop() {
    // Don't unmount the partition as there is no reason to do so
    return m_status = unregisterOTA();
}

Status Filesystem::getStatus() { return m_status; }

// todo: Implement OTA
Status Filesystem::registerOTA() { return Status::RUNNING; }
Status Filesystem::unregisterOTA() { return Status::STOPPED; }