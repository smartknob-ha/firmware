#include "DisplayDriver.hpp"

#include "display.hpp"
#include "display_drivers.hpp"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "gc9a01.hpp"
#include "hal/spi_types.h"

static spi_device_handle_t spi;
static size_t              num_queued_trans = 0;
static gpio_num_t          display_dc;

static constexpr size_t            SPI_QUEUE_SIZE     = 7;
static constexpr int               FLUSH_BIT          = (1 << (int) espp::display_drivers::Flags::FLUSH_BIT);
static constexpr int               DC_LEVEL_BIT       = (1 << (int) espp::display_drivers::Flags::DC_LEVEL_BIT);
static constexpr int               SPI_CLOCK_SPEED    = CONFIG_DISPLAY_SPI_CLOCK_SPEED * 1000 * 1000;
static constexpr spi_host_device_t SPI_BUS            = static_cast<const spi_host_device_t>(CONFIG_DISPLAY_SPI_BUS);
static constexpr bool              BACKLIGHT_ON_VALUE = true;
static constexpr bool              RESET_VALUE        = false;
static constexpr size_t            PIXEL_BUFFER_SIZE  = CONFIG_DISPLAY_WIDTH * 50;

#ifdef CONFIG_DISPLAY_FRAMEBUFFER_IN_PSRAM
static lv_color_t EXT_RAM_BSS_ATTR frameBuffer_0[PIXEL_BUFFER_SIZE];
static lv_color_t EXT_RAM_BSS_ATTR frameBuffer_1[PIXEL_BUFFER_SIZE];
#else
static lv_color_t frameBuffer_0[PIXEL_BUFFER_SIZE];
static lv_color_t frameBuffer_1[PIXEL_BUFFER_SIZE];
#endif
static std::unique_ptr<espp::Display> p_display;

// This function is called (in irq context!) just before a transmission starts.
// It will set the D/C line to the value indicated in the user field
// (DC_LEVEL_BIT).
static void IRAM_ATTR displaySpiPreTransfer(spi_transaction_t* t) {
    uint32_t user_flags = (uint32_t) (t->user);
    bool     dc_level   = user_flags & DC_LEVEL_BIT;
    gpio_set_level(display_dc, dc_level);
}

// This function is called (in irq context!) just after a transmission ends. It
// will indicate to lvgl that the next flush is ready to be done if the
// FLUSH_BIT is set.
static void IRAM_ATTR displaySpiPostTransfer(spi_transaction_t* t) {
    uint16_t user_flags   = (uint32_t) (t->user);
    bool     should_flush = user_flags & FLUSH_BIT;
    if (should_flush) {
        lv_disp_t* disp = _lv_refr_get_disp_refreshing();
        lv_disp_flush_ready(disp->driver);
    }
}

extern "C" void IRAM_ATTR displayWrite(const uint8_t* data, size_t length, uint32_t user_data) {
    if (length == 0) {
        return;
    }
    static spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = length * 8;
    t.tx_buffer = data;
    t.user      = (void*) user_data;
    spi_device_polling_transmit(spi, &t);
}

static void displayWaitForLines() {
    spi_transaction_t* rtrans;
    esp_err_t          ret;
    // Wait for all transactions to be done and get back the results.
    while (num_queued_trans) {
        // fmt::print("Waiting for {} lines\n", num_queued_trans);
        ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        if (ret != ESP_OK) {
            fmt::print("Could not get trans result: {} '{}'\n", ret, esp_err_to_name(ret));
        }
        num_queued_trans--;
        // We could inspect rtrans now if we received any info back. The LCD is treated as write-only,
        // though.
    }
}

void IRAM_ATTR displaySendLines(int xs, int ys, int xe, int ye, const uint8_t* data,
                                uint32_t user_data) {
    // if we haven't waited by now, wait here...
    displayWaitForLines();
    esp_err_t ret;
    // Transaction descriptors. Declared static so they're not allocated on the stack; we need this
    // memory even when this function is finished because the SPI driver needs access to it even while
    // we're already calculating the next line.
    static spi_transaction_t trans[6];
    // In theory, it's better to initialize trans and data only once and hang on to the initialized
    // variables. We allocate them on the stack, so we need to re-init them each call.
    for (int i = 0; i < 6; i++) {
        memset(&trans[i], 0, sizeof(spi_transaction_t));
        if ((i & 1) == 0) {
            // Even transfers are commands
            trans[i].length = 8;
            trans[i].user   = (void*) 0;
        } else {
            // Odd transfers are data
            trans[i].length = 8 * 4;
            trans[i].user   = (void*) DC_LEVEL_BIT;
        }
        trans[i].flags = SPI_TRANS_USE_TXDATA;
    }
    size_t length       = (xe - xs + 1) * (ye - ys + 1) * 2;
    trans[0].tx_data[0] = (uint8_t) espp::Gc9a01::Command::caset;
    trans[1].tx_data[0] = (xs) >> 8;
    trans[1].tx_data[1] = (xs) & 0xff;
    trans[1].tx_data[2] = (xe) >> 8;
    trans[1].tx_data[3] = (xe) & 0xff;
    trans[2].tx_data[0] = (uint8_t) espp::Gc9a01::Command::raset;
    trans[3].tx_data[0] = (ys) >> 8;
    trans[3].tx_data[1] = (ys) & 0xff;
    trans[3].tx_data[2] = (ye) >> 8;
    trans[3].tx_data[3] = (ye) & 0xff;
    trans[4].tx_data[0] = (uint8_t) espp::Gc9a01::Command::ramwr;
    trans[5].tx_buffer  = data;
    trans[5].length     = length * 8;
    // undo SPI_TRANS_USE_TXDATA flag
    trans[5].flags = 0;
    // we need to keep the dc bit set, but also add our flags
    trans[5].user = (void*) (DC_LEVEL_BIT | user_data);
    // Queue all transactions.
    for (int i = 0; i < 6; i++) {
        ret = spi_device_queue_trans(spi, &trans[i], portMAX_DELAY);
        if (ret != ESP_OK) {
            fmt::print("Couldn't queue trans: {} '{}'\n", ret, esp_err_to_name(ret));
        } else {
            num_queued_trans++;
        }
    }
    // When we are here, the SPI driver is busy (in the background) getting the
    // transactions sent. That happens mostly using DMA, so the CPU doesn't have
    // much to do here. We're not going to wait for the transaction to finish
    // because we may as well spend the time calculating the next line. When that
    // is done, we can call send_line_finish, which will wait for the transfers
    // to be done and check their status.
}

sdk::res DisplayDriver::initialize() {

    // If the display is already initialized, but stopped, we can just resume it
    if (m_initialized) {
        ESP_LOGD(TAG, "Resuming display driver");
        p_display->resume();
        m_status = sdk::ComponentStatus::RUNNING;
        return m_status;
    }
    m_status = sdk::ComponentStatus::INITIALIZING;
    ESP_LOGD(TAG, "Initializing display driver");

    // Used in callbacks, those can't take non-static members directly
    display_dc = m_config.display_dc;

    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.mosi_io_num     = m_config.spi_mosi;
    buscfg.miso_io_num     = -1;
    buscfg.sclk_io_num     = m_config.spi_sclk;
    buscfg.quadwp_io_num   = -1;
    buscfg.quadhd_io_num   = -1;
    buscfg.max_transfer_sz = PIXEL_BUFFER_SIZE * sizeof(lv_color_t);
    // Initialize the SPI bus
    auto ret = spi_bus_initialize(SPI_BUS, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    // create the spi device
    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.mode           = 0;
    devcfg.clock_speed_hz = SPI_CLOCK_SPEED;
    devcfg.input_delay_ns = 0;
    devcfg.spics_io_num   = m_config.spi_cs;
    devcfg.queue_size     = SPI_QUEUE_SIZE;
    devcfg.pre_cb         = displaySpiPreTransfer;
    devcfg.post_cb        = displaySpiPostTransfer;
    // Attach the LCD to the SPI bus
    ret = spi_bus_add_device(SPI_BUS, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

    // initialize the controller
    espp::Gc9a01::initialize(espp::display_drivers::Config{
            .lcd_write        = displayWrite,
            .lcd_send_lines   = displaySendLines,
            .reset_pin        = m_config.display_reset,
            .data_command_pin = m_config.display_dc,
            .reset_value      = RESET_VALUE,
            .invert_colors    = true,
            .offset_x         = 0,
            .offset_y         = 0,
            .mirror_x         = false,
            .mirror_y         = false,
    });

    auto displayConfig = espp::Display::NonAllocatingConfig{
            .vram0                     = frameBuffer_0,
            .vram1                     = frameBuffer_1,
            .width                     = CONFIG_DISPLAY_WIDTH,
            .height                    = CONFIG_DISPLAY_HEIGHT,
            .pixel_buffer_size         = PIXEL_BUFFER_SIZE,
            .flush_callback            = espp::Gc9a01::flush,
            .backlight_pin             = m_config.display_backlight,
            .backlight_on_value        = BACKLIGHT_ON_VALUE,
            .rotation                  = static_cast<espp::Display::Rotation>(m_config.rotation),
            .software_rotation_enabled = true};

    p_display = std::make_unique<espp::Display>(displayConfig);

    m_initialized = true;
    m_status      = sdk::ComponentStatus::RUNNING;
    ESP_LOGD(TAG, "Finished initializing display driver");
    return m_status;
}

void DisplayDriver::setBrightness(uint8_t brightness) {
    p_display->set_brightness(brightness / 255.0f);
}

sdk::res DisplayDriver::getStatus() {
    return m_status;
}

sdk::res DisplayDriver::run() {
    DisplayMsg displayMsg_;
    if (DisplayMsgQueue::dequeue(displayMsg_, 0) == pdTRUE) {
        setBrightness(displayMsg_.brightness);
    }
    return m_status;
}

sdk::res DisplayDriver::stop() {
    if (m_status != sdk::ComponentStatus::STOPPED) {
        return sdk::ComponentStatus::STOPPED;
    }

    ESP_LOGD(TAG, "Pausing display driver");

    // First turn off the backlight to hide any garbage from the user
    p_display->set_brightness(0.0f);

    // Now pause the display update task
    p_display->pause();

    m_status = sdk::ComponentStatus::STOPPED;
    return m_status;
}
