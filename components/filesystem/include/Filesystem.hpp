#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <etl/string.h>
#include <sys/_default_fcntl.h>

#include <expected>
#include <fstream>
#include <span>
#include <string>

#include "Component.hpp"
#include "HttpServer.hpp"
#include "esp_err.h"
#include "esp_log.h"

namespace sdk::Http {
    class Server;
}
class Filesystem final : public sdk::Component {
public:
    ~Filesystem() = default;

    explicit Filesystem(const sdk::Http::Server& httpServer) : m_httpServer(httpServer) {}

    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };
    Status          getStatus() override;
    Status          initialize() override;
    Status          run() override;
    Status          stop() override;

    // First is the size of the partition in bytes, second is the amount of used space in bytes
    using PartitionInfo = std::pair<size_t, size_t>;

    /**
     * Write a file to the filesystem
     * @param filepath The path to the file to write
     * @param data The data to write to the file
     * @return std::error_code
     */
    static std::error_code writeFile(const etl::string_view filepath, const etl::string_view data) noexcept {
        const int fd = open(filepath.data(), O_WRONLY | O_CREAT, 0644);
        if (fd == -1) {
            return std::error_code(errno, std::generic_category());
        }

        if (const auto ret = write(fd, data.data(), data.size()); ret == -1) {
            close(fd);
            return std::error_code(errno, std::generic_category());
        }

        close(fd);
        return std::error_code();
    }

    /**
     * Read a file from the filesystem
     * @tparam FILESIZE Length of the data to read
     * @param filepath The path to the file to read
     * @return std::expected<etl::string<FILESIZE>, std::error_code>
     */
    template<size_t FILESIZE>
    static std::expected<etl::string<FILESIZE>, std::error_code> readFile(const etl::string_view filepath) noexcept {
        auto filesize = fileSize(filepath);
        if (!filesize.has_value()) {
            return std::unexpected(filesize.error());
        }
        assert(filesize.value() <= FILESIZE && "File is too large to read into buffer");

        std::ifstream inFile(filepath.data(), std::ios::in | std::ios::binary);
        if (!inFile) {
            return std::unexpected(std::error_code(errno, std::generic_category()));
        }

        std::array<char, FILESIZE> buffer;
        inFile.read(buffer.data(), buffer.size());

        if (inFile.bad()) {
            return std::unexpected(std::error_code(errno, std::generic_category()));
        }
        return etl::string_ext(buffer.data(), buffer.data(), buffer.size());
    }

    /**
     * Get information about the static assets partition
     * @return std::expected<PartitionInfo, std::error_code>
     */
    static std::expected<PartitionInfo, std::error_code> staticAssetsInfo();

    /**
     * Get information about the OTA assets partition
     * @return std::expected<PartitionInfo, std::error_code>
     */
    static std::expected<PartitionInfo, std::error_code> otaAssetsInfo();

private:
    static constexpr char TAG[] = "Filesystem";

    static inline bool partitionsAreMounted = false;

    static inline auto m_status = Status::UNINITIALIZED;

    static inline etl::string<13> m_currentBootPartitionLabel = "";
    static inline etl::string<13> m_currentAssetsLabel        = "";

    static constexpr auto OTA_A_LABEL                  = "ota_a";
    static constexpr auto OTA_A_MAIN_PARTITION_LABEL   = "ota_a_assets";
    static constexpr auto MAIN_PARTITION_MOUNT_POINT   = "/main";
    static constexpr auto OTA_B_LABEL                  = "ota_b";
    static constexpr auto OTA_B_MAIN_PARTITION_LABEL   = "ota_b_assets";
    static constexpr auto STATIC_PARTITION_LABEL       = "static_assets";
    static constexpr auto STATIC_PARTITION_MOUNT_POINT = "/static_assets";

    struct OtaInfoHeader {
        size_t                  appSize;
        std::array<uint8_t, 32> appSha;
        size_t                  otaAssetsSize;
        std::array<uint8_t, 32> otaAssetsSha;
        bool                    hasStaticAssets;
        size_t                  staticAssetsSize;
        std::array<uint8_t, 32> staticAssetsSha;
        char                    firmwareVersion[20];
    };

    // The HTTP server to register the OTA handler to
    const sdk::Http::Server& m_httpServer;

    /**
     * Get the size of a file
     * @param filepath The path to the file to get the size of
     * @return std::expected<size_t, std::error_code>
     */
    static std::expected<size_t, std::error_code> fileSize(const etl::string_view& filepath) noexcept {
        struct stat st;
        if (stat(filepath.data(), &st) == -1) {
            return std::unexpected(std::error_code(errno, std::generic_category()));
        }
        return st.st_size;
    }

    /**
     * Handles incoming OTA requests
     * @param req The incoming request
     * @return esp_err_t
     */
    static esp_err_t otaHandler(httpd_req_t* req);

    /**
     * Write a partition with OTA data. First erase the partition, then writes the data to it and verifies the SHA256 hash
     * @param req The request to write the partition from
     * @param partition The partition to write to
     * @param updateSize The size of the update
     * @param sha256 The SHA256 hash of the update
     * @param buffer The buffer to be used for receiving data and writing to the partition
     * @return esp_err_t
     */
    static esp_err_t otaWritePartition(httpd_req_t* req, const esp_partition_t* partition, size_t updateSize, std::span<uint8_t> sha256, std::span<char> buffer);

    /**
     * Restart task that waits a second, and restarts the device
     * Required for the OTA handler to finish its response before restarting
     */
    static void restartTask(void*);

    /**
     * Receive a buffer from a request, will keep receiving data until the buffer is at the desired length
     * @param req The request to receive the buffer from
     * @param buffer The buffer to receive the data into
     * @param length The length of the buffer
     * @return esp_err_t
     */
    static esp_err_t receiveBuffer(httpd_req_t* req, std::span<char> buffer, size_t length);

    /**
     * TODO: Determine & implement version comparison
     * Verify the version number of the incoming OTA update
     * @param newVersion The version number of the incoming OTA update
     * @return bool True if the version number is valid, false otherwise
     */
    static bool verifyVersionNumber(const etl::string_view newVersion);

    /**
     * Get information about a partition
     * @param partitionLabel The label of the partition to get information about
     * @return std::expected<PartitionInfo, std::error_code>
     */
    static std::expected<PartitionInfo, std::error_code> partitionInfo(etl::string_view partitionLabel);

    /**
     * Register the OTA handler at CONFIG_FILESYSTEM_OTA_POST_PATH
     * @return std::error_code of an esp_err_t
     */
    std::error_code registerOTA() const;


    /**
     * Handle an OTA error, logs and sends the message + error to the client
     * @param req The request that caused the error
     * @param error The error code to log
     * @param message The message to log
     * @param code The HTTP error code to send
     */
    static esp_err_t handleOtaError(httpd_req_t* req, esp_err_t error, etl::string_view message, httpd_err_code_t code);

    /**
     * Handle an OTA error, logs and sends the message to the client
     * @param req The request that caused the error
     * @param message The message to log
     * @param code The HTTP error code to send
     */
    static void handleOtaError(httpd_req_t* req, etl::string_view message, httpd_err_code_t code);

    /**
     * Convert a SHA256 hash to a string
     * @param sha256 The SHA256 hash to convert
     * @return etl::string<64> The SHA256 hash as a hexadecimal string
     */
    static etl::string<64> sha256ToString(const std::span<uint8_t> sha256);

    /**
     * Verify the SHA256 hash of an entire partition
     * @param partition The partition to verify
     * @param sha256 The SHA256 hash to verify against
     * @return bool True if the SHA256 hash matches, false otherwise
     */
    static bool verifyEntirePartition(const esp_partition_t* partition, std::span<uint8_t> sha256);
};

#endif /* FILESYSTEM_HPP */