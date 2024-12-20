#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <sys/_default_fcntl.h>

#include <string>
#include <fstream>

#include "Component.hpp"
#include "esp_err.h"
#include "esp_log.h"

#include <etl/string.h>

class Filesystem final : public sdk::Component {
public:
    Filesystem()  = default;
    ~Filesystem() = default;

    /* Component override functions */
    etl::string<50> getTag() override { return TAG; };
    Status          getStatus() override;
    Status          initialize() override;
    Status          run() override;
    Status          stop() override;

    // First is the size of the partition in bytes, second is the amount of used space in bytes
    using PartitionInfo = std::pair<std::size_t, std::size_t>;

    /**
     * Write a file to the filesystem
     * @tparam FILEPATH_SIZE Length of the filepath
     * @tparam FILESIZE Length of the data to write
     * @param filepath The path to the file to write
     * @param data The data to write to the file
     * @return std::error_code
     */
    template<std::size_t FILEPATH_SIZE, std::size_t FILESIZE>
    static std::error_code writeFile(const etl::string<FILEPATH_SIZE>& filepath, const etl::string<FILESIZE>& data) noexcept {
        const int fd = open(filepath.c_str(), O_WRONLY | O_CREAT, 0644);
        if (fd == -1) { return std::error_code(errno, std::generic_category()); }

        if (const auto ret = write(fd, data.c_str(), data.size()); ret == -1) {
            close(fd);
            return std::error_code(errno, std::generic_category());
        }

        close(fd);
        return std::error_code();
    }

    /**
     * Read a file from the filesystem
     * @tparam FILEPATH_SIZE Length of the filepath
     * @tparam FILESIZE Length of the data to read
     * @param filepath The path to the file to read
     * @return std::expected<etl::string<FILESIZE>, std::error_code>
     */
    template<std::size_t FILEPATH_SIZE, std::size_t FILESIZE>
    static std::expected<etl::string<FILESIZE>, std::error_code> readFile(const etl::string<FILEPATH_SIZE>& filepath) noexcept {
        auto filesize = fileSize(filepath);
        if (!filesize.has_value()) { return std::unexpected(filesize.error()); }
        assert(filesize.value() <= FILESIZE && "File is too large to read into buffer");

        std::ifstream inFile(filepath.c_str(), std::ios::in | std::ios::binary);
        if (!inFile) { return std::unexpected(std::error_code(errno, std::generic_category())); }

        std::array<char, FILESIZE> buffer;
        inFile.read(buffer.data(), buffer.size());

        if (inFile.bad()) { return std::unexpected(std::error_code(errno, std::generic_category())); }
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

    static inline etl::string<13> otaAssetsLabel = "";

    static constexpr auto OTA_A_LABEL                  = "ota_a";
    static constexpr auto OTA_A_MAIN_PARTITION_LABEL   = "ota_a_assets";
    static constexpr auto MAIN_PARTITION_MOUNT_POINT   = "/main";
    static constexpr auto OTA_B_LABEL                  = "ota_b";
    static constexpr auto OTA_B_MAIN_PARTITION_LABEL   = "ota_b_assets";
    static constexpr auto STATIC_PARTITION_LABEL       = "static_assets";
    static constexpr auto STATIC_PARTITION_MOUNT_POINT = "/static_assets";

    /**
     * Get the size of a file
     * @tparam FILEPATH_SIZE Length of the filepath
     * @param filepath The path to the file to get the size of
     * @return std::expected<std::size_t, std::error_code>
     */
    template<std::size_t FILEPATH_SIZE>
    static std::expected<std::size_t, std::error_code> fileSize(const etl::string<FILEPATH_SIZE>& filepath) noexcept {
        struct stat st;
        if (stat(filepath.c_str(), &st) == -1) { return std::unexpected(std::error_code(errno, std::generic_category())); }

        return st.st_size;
    }

    /**
     * Get information about a partition
     * @param partitionLabel The label of the partition to get information about
     * @return std::expected<PartitionInfo, std::error_code>
     */
    static std::expected<PartitionInfo, std::error_code> partitionInfo(const etl::string<20>& partitionLabel);

    static Status registerOTA();
    static Status unregisterOTA();
};

#endif /* FILESYSTEM_HPP */