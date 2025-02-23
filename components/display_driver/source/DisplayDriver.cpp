#include "DisplayDriver.hpp"

#include "display.hpp"
#include "display_drivers.hpp"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "sh8601.hpp"
#include "hal/spi_types.h"

#define INIT_RETURN_ON_ERROR(err)                                      \
    do {                                                               \
        if (err != ESP_OK) {                                           \
            ESP_LOGE(TAG, "Failed to init: %s", esp_err_to_name(err)); \
            m_err           = err;                                     \
            return m_status = Status::ERROR;                           \
        }                                                              \
    } while (0)

using Pixel   = lv_color_t;
using Display = espp::Display<Pixel>;
using Driver  = espp::Sh8601;

static spi_device_handle_t spi;
static size_t              num_queued_trans = 0;
static gpio_num_t          display_dc;

static constexpr size_t            SPI_QUEUE_SIZE     = 10;
static constexpr int               FLUSH_BIT          = (1 << (int) espp::display_drivers::Flags::FLUSH_BIT);
static constexpr int               DC_LEVEL_BIT       = (1 << (int) espp::display_drivers::Flags::DC_LEVEL_BIT);
static constexpr int               SPI_CLOCK_SPEED    = CONFIG_DISPLAY_SPI_CLOCK_SPEED * 1000 * 1000;
static constexpr spi_host_device_t SPI_BUS            = static_cast<const spi_host_device_t>(CONFIG_DISPLAY_SPI_BUS);
static constexpr bool              BACKLIGHT_ON_VALUE = true;
static constexpr bool              RESET_VALUE        = false;
static constexpr size_t            PIXEL_BUFFER_SIZE  = CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT / 10;

#ifdef CONFIG_DISPLAY_FRAMEBUFFER_IN_PSRAM
static Pixel EXT_RAM_BSS_ATTR frameBuffer_0[PIXEL_BUFFER_SIZE];
static Pixel EXT_RAM_BSS_ATTR frameBuffer_1[PIXEL_BUFFER_SIZE];
#else
static Pixel frameBuffer_0[PIXEL_BUFFER_SIZE];
static Pixel frameBuffer_1[PIXEL_BUFFER_SIZE];
#endif
static std::unique_ptr<Display> p_display;

// This function is called (in irq context!) just after a transmission ends. It
// will indicate to lvgl that the next flush is ready to be done if the
// FLUSH_BIT is set.
static void IRAM_ATTR displaySpiPostTransfer(spi_transaction_t* t) {
    uint16_t user_flags   = reinterpret_cast<uint32_t>(t->user);
    bool     should_flush = user_flags & FLUSH_BIT;
    if (should_flush) {
        lv_display_flush_ready(lv_display_get_default());
    }
}

extern "C" void IRAM_ATTR writeCommand(uint8_t command, const uint8_t* parameters, size_t length, uint32_t user_data) {
    static spi_transaction_t t = {};

    t.cmd   = 0x02;
    t.addr  = static_cast<uint32_t>(command) << 8;
    t.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    if (length > 0) {
        for (size_t i = 0; i < length; i++) {
            t.tx_data[i] = parameters[i];
        }
        t.length = length * 8;
        t.flags  = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR | SPI_TRANS_USE_TXDATA;
    } else {
        t.tx_buffer = nullptr;
        t.length    = 0;
    }
    t.user = reinterpret_cast<void*>(user_data);

    auto ret = spi_device_acquire_bus(spi, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE("CMD", "Failed to acquire bus: %s", esp_err_to_name(ret));
    }
    ret = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE("CMD", "Failed to send command: %s", esp_err_to_name(ret));
    }
    spi_device_release_bus(spi);
}

void DisplayDriver::waitForLines() {
    spi_transaction_t* transaction_result = nullptr;
    // Wait for all transactions to be done and get back the results.
    while (num_queued_trans) {
        auto ret = spi_device_get_trans_result(spi, &transaction_result, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Couldn't get transaction result: %s", esp_err_to_name(ret));
        }
        num_queued_trans--;
        // We could inspect rtrans now if we received any info back. The LCD is treated as write-only,
        // though.
    }
}

void DisplayDriver::sendLines(const int xStart, const int yStart, const int xEnd, const int yEnd, const uint8_t* data,
                              const uint32_t user_data) {
    static bool initialized = false;

    // Transaction descriptors. Declared static so they're not allocated on the stack; we need this
    // memory even when this function is finished because the SPI driver needs access to it even while
    // we're already calculating the next line.
    static std::array<spi_transaction_t, SPI_QUEUE_SIZE> transactions = {};

    static size_t max_transfer_size = 0;

    // Initialize the above SPI transactions, this only has to be done once
    if (!initialized) {
        transactions[0].cmd    = 0x02;
        transactions[0].addr   = static_cast<uint8_t>(Driver::Command::caset) << 8;
        transactions[0].length = 4 * 8;
        transactions[0].flags  = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR | SPI_TRANS_USE_TXDATA;

        transactions[1].cmd    = 0x02;
        transactions[1].addr   = static_cast<uint8_t>(Driver::Command::paset) << 8;
        transactions[1].length = 4 * 8;
        transactions[1].flags  = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR | SPI_TRANS_USE_TXDATA;

        transactions[2].cmd    = 0x02;
        transactions[2].addr   = static_cast<uint8_t>(Driver::Command::ramwr) << 8;
        transactions[2].length = 0;
        transactions[2].flags  = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;

        transactions[3].flags = SPI_TRANS_MODE_QIO;
        transactions[3].cmd   = 0x32;
        transactions[3].addr  = 0x003C00;

        spi_bus_get_max_transaction_len(SPI_BUS, &max_transfer_size);
        initialized = true;
    }

    const size_t length = (xEnd - xStart + 1) * (yEnd - yStart + 1) * sizeof(Pixel);

    // Wait for previous flush transactions to finish
    waitForLines();

    // Bitwise operations are to split the coordinates into two 8-bit values
    transactions[0].tx_data[0] = (xStart) >> 8;
    transactions[0].tx_data[1] = (xStart) & 0xff;
    transactions[0].tx_data[2] = (xEnd) >> 8;
    transactions[0].tx_data[3] = (xEnd) & 0xff;

    transactions[1].tx_data[0] = (yStart) >> 8;
    transactions[1].tx_data[1] = (yStart) & 0xff;
    transactions[1].tx_data[2] = (yEnd) >> 8;
    transactions[1].tx_data[3] = (yEnd) & 0xff;

    size_t remaining = length;
    size_t index     = 3; // Start at 3 because the first 3 transactions are required for setup
    while (remaining && index < transactions.size()) {
        const size_t transfer_size = std::min(remaining, max_transfer_size);
        // Move the data pointer to the max_transfer_size times the amount of transactions already created
        transactions[index].tx_buffer = data + max_transfer_size * (index - 3);
        transactions[index].length    = transfer_size * 8; // Length is in bits
        if (index == 3) {
            transactions[index].flags = SPI_TRANS_MODE_QIO | SPI_TRANS_CS_KEEP_ACTIVE;
        } else {
            // Only the first transaction should transfer the command and address
            transactions[index].flags = SPI_TRANS_MODE_QIO | SPI_TRANS_CS_KEEP_ACTIVE | SPI_TRANS_VARIABLE_CMD |
                                        SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
        }

        remaining -= transfer_size;
        index++;
    }

    // Set the flush bit on the last transaction, index - 1 as index is already incremented
    transactions[index - 1].user = reinterpret_cast<void*>(user_data);
    // Have the final pixel transaction stop asserting the CS line
    transactions[index - 1].flags &= ~SPI_TRANS_CS_KEEP_ACTIVE;

    // Acquire the SPI bus, required for the SPI_TRANS_CS_KEEP_ACTIVE flag
    auto ret = spi_device_acquire_bus(spi, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Couldn't acquire bus: %s", esp_err_to_name(ret));
        return;
    }

    // Queue all used transactions
    for (int i = 0; i < index; i++) {
        esp_err_t ret = spi_device_queue_trans(spi, &transactions[i], portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Couldn't queue transaction: %s", esp_err_to_name(ret));
        } else {
            num_queued_trans++;
        }
    }
    spi_device_release_bus(spi);
    // When we are here, the SPI driver is busy (in the background) getting the
    // transactions sent. That happens mostly using DMA, so the CPU doesn't have
    // much to do here. We're not going to wait for the transaction to finish
    // because we may as well spend the time calculating the next line. When that
    // is done, we can call send_line_finish, which will wait for the transfers
    // to be done and check their status.
}

using Status = sdk::Component::Status;
using res    = sdk::Component::res;

Status DisplayDriver::initialize() {

    // If the display is already initialized, but stopped, we can just resume it
    if (m_initialized) {
        ESP_LOGD(TAG, "Resuming display driver");
        p_display->resume();
        return m_status = Status::RUNNING;
    }

    constexpr auto gpio_output_pin_sel = (1ULL << static_cast<gpio_num_t>(3));


    constexpr gpio_config_t o_conf{.pin_bit_mask = gpio_output_pin_sel,
                                   .mode         = GPIO_MODE_OUTPUT,
                                   .pull_up_en   = GPIO_PULLUP_DISABLE,
                                   .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                   .intr_type    = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&o_conf));

    gpio_set_level(GPIO_NUM_3, 1);


    m_status = Status::INITIALIZING;
    ESP_LOGD(TAG, "Initializing display driver");

    // Used in callbacks, those can't take non-static members directly
    display_dc = m_config.display_dc;

    spi_bus_config_t buscfg = {};
    // buscfg.data0_io_num = 11;
    // buscfg.data1_io_num = 13;
    buscfg.mosi_io_num  = 11;
    buscfg.miso_io_num  = 13;
    buscfg.data2_io_num = 7;
    buscfg.data3_io_num = 14;
    buscfg.sclk_io_num  = 12;

    buscfg.data_io_default_level = true;

    buscfg.flags = SPICOMMON_BUSFLAG_MASTER;
    // buscfg.flags           = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;
    buscfg.max_transfer_sz = 32768; // This is the hardware limit of a single DMA transfer

    // Initialize the SPI bus
    auto ret = spi_bus_initialize(SPI_BUS, &buscfg, SPI_DMA_CH_AUTO);
    INIT_RETURN_ON_ERROR(ret);

    // create the spi device
    spi_device_interface_config_t devcfg = {};
    devcfg.mode                          = 0;
    devcfg.clock_speed_hz                = SPI_CLOCK_SPEED;
    devcfg.input_delay_ns                = 0;
    devcfg.spics_io_num                  = 10;
    devcfg.queue_size                    = SPI_QUEUE_SIZE;
    devcfg.flags                         = SPI_DEVICE_HALFDUPLEX;
    devcfg.post_cb                       = displaySpiPostTransfer;

    devcfg.command_bits = 8;
    devcfg.address_bits = 24;

    // Attach the LCD to the SPI bus
    ret = spi_bus_add_device(SPI_BUS, &devcfg, &spi);
    INIT_RETURN_ON_ERROR(ret);

    // initialize the controller
    Driver::initialize(espp::display_drivers::Config{
            .write_command    = writeCommand,
            .lcd_send_lines   = sendLines,
            .reset_pin        = static_cast<gpio_num_t>(4),
            .data_command_pin = m_config.display_dc,
            .reset_value      = RESET_VALUE,
            .invert_colors    = false,
            .offset_x         = 0,
            .offset_y         = 0,
            .mirror_x         = false,
            .mirror_y         = false,
    });

    auto displayConfig = Display::NonAllocatingConfig{
            .vram0                     = frameBuffer_0,
            .vram1                     = frameBuffer_1,
            .width                     = CONFIG_DISPLAY_WIDTH,
            .height                    = CONFIG_DISPLAY_HEIGHT,
            .pixel_buffer_size         = PIXEL_BUFFER_SIZE,
            .flush_callback            = Driver::flush,
            .rotation_callback         = Driver::rotate,
            .rotation                  = static_cast<espp::DisplayRotation>(m_config.rotation),
            .software_rotation_enabled = true};

    p_display = std::make_unique<Display>(displayConfig);

    m_initialized = true;
    ESP_LOGD(TAG, "Finished initializing display driver");
    return m_status = Status::RUNNING;
}

void DisplayDriver::setBrightness(uint8_t brightness) {
    Driver::set_brightness(brightness);
}

Status DisplayDriver::run() {
    DisplayMsg displayMsg_;
    if (DisplayMsgQueue::dequeue(displayMsg_, 0) == pdTRUE) {
        setBrightness(displayMsg_.brightness);
    }
    return m_status;
}

Status DisplayDriver::stop() {
    if (m_status == Status::STOPPED) {
        return Status::STOPPED;
    }

    ESP_LOGD(TAG, "Pausing display driver");

    // First turn off the backlight to hide any garbage from the user
    Driver::set_brightness(0.0f);

    // Now pause the display update task
    p_display->pause();

    return m_status = Status::STOPPED;
}
