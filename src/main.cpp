#include <rom/ets_sys.h>

#include <chrono>
#include <memory>
#include <vector>

#include "display.hpp"
#include "driver/spi_master.h"
#include "hal/spi_types.h"

#define CONFIG_T_ENCODER_PRO 1
// #define CONFIG_SMARTKNOB_HA 1
#if CONFIG_HARDWARE_WROVER_KIT
#include "ili9341.hpp"
static constexpr int DC_PIN_NUM = 21;
using Display                   = espp::Display<lv_color16_t>;
using DisplayDriver             = espp::Ili9341;
#elif CONFIG_HARDWARE_TTGO
#include "st7789.hpp"
static constexpr int DC_PIN_NUM = 16;
using Display                   = espp::Display<lv_color16_t>;
using DisplayDriver             = espp::St7789;
#elif CONFIG_HARDWARE_BOX
#include "st7789.hpp"
static constexpr int DC_PIN_NUM = 4;
using Display                   = espp::Display<lv_color16_t>;
using DisplayDriver             = espp::St7789;
#elif CONFIG_SMARTKNOB_HA
#include "gc9a01.hpp"
static constexpr int DC_PIN_NUM = 16;
using Display                   = espp::Display<lv_color16_t>;
using DisplayDriver             = espp::Gc9a01;
#elif CONFIG_T_ENCODER_PRO
#define DISPLAY_BUS_QSPI 1
#include "sh8601.hpp"
using Display       = espp::Display<lv_color16_t>;
using DisplayDriver = espp::Sh8601;
#else
#error "Misconfigured hardware!"
#endif

// for the example
#include "gui.hpp"

using namespace std::chrono_literals;

static spi_device_handle_t spi;
static const int           spi_queue_size   = 7;
static size_t              num_queued_trans = 0;

static auto spi_num = SPI2_HOST;

// the user flag for the callbacks does two things:
// 1. Provides the GPIO level for the data/command pin, and
// 2. Sets some bits for other signaling (such as LVGL FLUSH)
static constexpr int FLUSH_BIT    = (1 << (int) espp::display_drivers::Flags::FLUSH_BIT);
static constexpr int DC_LEVEL_BIT = (1 << (int) espp::display_drivers::Flags::DC_LEVEL_BIT);

static void lcd_wait_lines();

//! [pre_transfer_callback example]
// This function is called (in irq context!) just before a transmission starts.
// It will set the D/C line to the value indicated in the user field
// (DC_LEVEL_BIT).
static void IRAM_ATTR lcd_spi_pre_transfer_callback(spi_transaction_t* t) {
#ifndef CONFIG_T_ENCODER_PRO
    uint32_t user_flags = (uint32_t) (t->user);
    bool     dc_level   = user_flags & DC_LEVEL_BIT;
    gpio_set_level((gpio_num_t) DC_PIN_NUM, dc_level);
#endif
}
//! [pre_transfer_callback example]

//! [post_transfer_callback example]
// This function is called (in irq context!) just after a transmission ends. It
// will indicate to lvgl that the next flush is ready to be done if the
// FLUSH_BIT is set.
static void IRAM_ATTR lcd_spi_post_transfer_callback(spi_transaction_t* t) {
    uint16_t user_flags   = (uint32_t) (t->user);
    bool     should_flush = user_flags & FLUSH_BIT;
    if (should_flush) {
        ets_printf("Notifying lvgl..\n");
        lv_display_flush_ready(lv_display_get_default());
        ets_printf("Finished notifying lvgl..\n");
    }
}
//! [post_transfer_callback example]

//! [polling_transmit example]
#ifdef DISPLAY_BUS_QSPI
extern "C" void IRAM_ATTR write_command(uint8_t command, const uint8_t* parameters, size_t length, uint32_t user_data) {
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

    printf("Sending command\nAcquiring bus..\n");
    auto ret = spi_device_acquire_bus(spi, portMAX_DELAY);
    if (ret != ESP_OK) {
        printf("Failed to acquire bus: %s\n", esp_err_to_name(ret));
        return;
    }
    // lcd_wait_lines();

    ret = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK) {
        printf("Failed to send command: %s\n", esp_err_to_name(ret));
    }
    // num_queued_trans++;
    spi_device_release_bus(spi);
    printf("Finished sending command\nReleasing bus..\n");
}
#else
extern "C" void IRAM_ATTR write_command(uint8_t command, const uint8_t* data, size_t length, uint32_t user_data) {
    static spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8;
    t.tx_buffer = &command;
    t.user      = reinterpret_cast<void*>(user_data);
    if (length > 0) {
        spi_device_polling_transmit(spi, &t);
        t.length    = length * 8;
        t.tx_buffer = data;
        t.user      = reinterpret_cast<void*>(user_data | (1 << static_cast<int>(espp::display_drivers::Flags::DC_LEVEL_BIT)));
    }
    spi_device_polling_transmit(spi, &t);
}
#endif

//! [polling_transmit example]

//! [queued_transmit example]
static void lcd_wait_lines() {
    spi_transaction_t* rtrans;
    esp_err_t          ret;
    // Wait for all transactions to be done and get back the results.
    printf("Waiting for %d queued transactions\n", num_queued_trans);
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

    printf("Finished waiting queued transactions\n");
}


#ifdef DISPLAY_BUS_QSPI
void IRAM_ATTR lcd_send_lines(const int xStart, const int yStart, const int xEnd, const int yEnd, const uint8_t* data,
                              const uint32_t user_data) {
    lv_display_flush_ready(lv_display_get_default());
    return;

    if (data == nullptr) {
        printf("Data is null\n");
        return;
    }
    static bool initialized = false;

    // Transaction descriptors. Declared static so they're not allocated on the stack; we need this
    // memory even when this function is finished because the SPI driver needs access to it even while
    // we're already calculating the next line.
    static std::array<spi_transaction_t, spi_queue_size> transactions = {};

    static size_t max_transfer_size = 0;

    // Initialize the above SPI transactions, this only has to be done once
    if (!initialized) {
        transactions[0].cmd  = 0x02;
        transactions[0].addr = static_cast<uint8_t>(DisplayDriver::Command::caset) << 8;

        transactions[0].length = 4 * 8;
        transactions[0].flags  = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR | SPI_TRANS_USE_TXDATA;

        transactions[1].cmd    = 0x02;
        transactions[1].addr   = static_cast<uint8_t>(DisplayDriver::Command::paset) << 8;
        transactions[1].length = 4 * 8;
        transactions[1].flags  = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR | SPI_TRANS_USE_TXDATA;

        transactions[2].cmd    = 0x02;
        transactions[2].addr   = static_cast<uint8_t>(DisplayDriver::Command::ramwr) << 8;
        transactions[2].length = 0;
        transactions[2].flags  = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;

        transactions[3].flags = SPI_TRANS_MODE_QIO;
        transactions[3].cmd   = 0x32;
        transactions[3].addr  = 0x003C00;

        spi_bus_get_max_transaction_len(spi_num, &max_transfer_size);
        initialized = true;
    }

    const size_t length = (xEnd - xStart + 1) * (yEnd - yStart + 1) * sizeof(lv_color_t);

    if (length == 0) {
        printf("Length is 0\n");
        return;
    }

    // Wait for previous flush transactions to finish
    lcd_wait_lines();

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
        if (index == transactions.size()) {
            printf("Too much data to send\n");
            break;
        }
    }

    printf("Transactions index: %d\n", index);

    // Set the flush bit on the last transaction, index - 1 as index is already incremented
    transactions[index - 1].user = reinterpret_cast<void*>(user_data);
    // Have the final pixel transaction stop asserting the CS line
    transactions[index - 1].flags &= ~SPI_TRANS_CS_KEEP_ACTIVE;

    // Acquire the SPI bus, required for the SPI_TRANS_CS_KEEP_ACTIVE flag
    auto ret = spi_device_acquire_bus(spi, portMAX_DELAY);
    if (ret != ESP_OK) {
        fmt::print("Couldn't acquire bus: {}", esp_err_to_name(ret));
        return;
    }

    // Queue all used transactions
    for (int i = 0; i < index; i++) {
        esp_err_t ret = spi_device_queue_trans(spi, &transactions[i], portMAX_DELAY);
        if (ret != ESP_OK) {
            fmt::print("Couldn't queue transaction: {}", esp_err_to_name(ret));
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
#else
void IRAM_ATTR lcd_send_lines(int xs, int ys, int xe, int ye, const uint8_t* data,
                              uint32_t user_data) {
    // if we haven't waited by now, wait here...
    lcd_wait_lines();
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
    size_t length = (xe - xs + 1) * (ye - ys + 1) * 2;
#if CONFIG_HARDWARE_WROVER_KIT
    trans[0].tx_data[0] = (uint8_t) espp::Ili9341::Command::caset;
#elif CONFIG_HARDWARE_TTGO || CONFIG_HARDWARE_BOX
    trans[0].tx_data[0] = (uint8_t) espp::St7789::Command::caset;
#elif CONFIG_SMARTKNOB_HA
    trans[0].tx_data[0] = (uint8_t) espp::Gc9a01::Command::caset;
#endif
    trans[1].tx_data[0] = (xs) >> 8;
    trans[1].tx_data[1] = (xs) & 0xff;
    trans[1].tx_data[2] = (xe) >> 8;
    trans[1].tx_data[3] = (xe) & 0xff;
#if CONFIG_HARDWARE_WROVER_KIT
    trans[2].tx_data[0] = (uint8_t) espp::Ili9341::Command::raset;
#elif CONFIG_HARDWARE_TTGO || CONFIG_HARDWARE_BOX
    trans[2].tx_data[0] = (uint8_t) espp::St7789::Command::raset;
#elif CONFIG_SMARTKNOB_HA
    trans[2].tx_data[0] = (uint8_t) espp::Gc9a01::Command::raset;
#endif
    trans[3].tx_data[0] = (ys) >> 8;
    trans[3].tx_data[1] = (ys) & 0xff;
    trans[3].tx_data[2] = (ye) >> 8;
    trans[3].tx_data[3] = (ye) & 0xff;
#if CONFIG_HARDWARE_WROVER_KIT
    trans[4].tx_data[0] = (uint8_t) espp::Ili9341::Command::ramwr;
#elif CONFIG_HARDWARE_TTGO || CONFIG_HARDWARE_BOX
    trans[4].tx_data[0] = (uint8_t) espp::St7789::Command::ramwr;
#elif CONFIG_SMARTKNOB_HA
    trans[4].tx_data[0] = (uint8_t) espp::Gc9a01::Command::ramwr;
#endif
    trans[5].tx_buffer = data;
    trans[5].length    = length * 8;
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
//! [queued_transmit example]
#endif

extern "C" void app_main(void) {
    size_t num_seconds_to_run = 10;
    {

        /**
         *   NOTE: for a reference of what display controllers exist on the different
         *   espressif development boards, see:
         *   https://github.com/espressif/esp-bsp/blob/master/LCD.md
         */

#if CONFIG_HARDWARE_WROVER_KIT
        //! [wrover_kit_config example]
        static constexpr std::string_view dev_kit            = "ESP-WROVER-DevKit";
        int                               clock_speed        = 20 * 1000 * 1000;
        gpio_num_t                        mosi               = GPIO_NUM_23;
        gpio_num_t                        sclk               = GPIO_NUM_19;
        gpio_num_t                        spics              = GPIO_NUM_22;
        gpio_num_t                        reset              = GPIO_NUM_18;
        gpio_num_t                        dc_pin             = (gpio_num_t) DC_PIN_NUM;
        gpio_num_t                        backlight          = GPIO_NUM_5;
        size_t                            width              = 320;
        size_t                            height             = 240;
        size_t                            pixel_buffer_size  = 16384;
        bool                              backlight_on_value = false;
        bool                              reset_value        = false;
        bool                              invert_colors      = false;
        int                               offset_x           = 0;
        int                               offset_y           = 0;
        bool                              mirror_x           = false;
        bool                              mirror_y           = false;
        auto                              rotation           = espp::DisplayRotation::LANDSCAPE;
        //! [wrover_kit_config example]
#elif CONFIG_HARDWARE_TTGO
        //! [ttgo_config example]
        static constexpr std::string_view dev_kit            = "TTGO T-Display";
        int                               clock_speed        = 60 * 1000 * 1000;
        gpio_num_t                        mosi               = GPIO_NUM_19;
        gpio_num_t                        sclk               = GPIO_NUM_18;
        gpio_num_t                        spics              = GPIO_NUM_5;
        gpio_num_t                        reset              = GPIO_NUM_23;
        gpio_num_t                        dc_pin             = (gpio_num_t) DC_PIN_NUM;
        gpio_num_t                        backlight          = GPIO_NUM_4;
        size_t                            width              = 240;
        size_t                            height             = 135;
        size_t                            pixel_buffer_size  = 12800;
        bool                              backlight_on_value = false;
        bool                              reset_value        = false;
        bool                              invert_colors      = false;
        int                               offset_x           = 40;
        int                               offset_y           = 53;
        bool                              mirror_x           = false;
        bool                              mirror_y           = false;
        auto                              rotation           = espp::DisplayRotation::PORTRAIT;
        //! [ttgo_config example]
#elif CONFIG_HARDWARE_BOX
        //! [box_config example]
        static constexpr std::string_view dev_kit            = "ESP32-S3-BOX";
        int                               clock_speed        = 60 * 1000 * 1000;
        gpio_num_t                        mosi               = GPIO_NUM_6;
        gpio_num_t                        sclk               = GPIO_NUM_7;
        gpio_num_t                        spics              = GPIO_NUM_5;
        gpio_num_t                        reset              = GPIO_NUM_48;
        gpio_num_t                        dc_pin             = (gpio_num_t) DC_PIN_NUM;
        gpio_num_t                        backlight          = GPIO_NUM_45;
        size_t                            width              = 320;
        size_t                            height             = 240;
        size_t                            pixel_buffer_size  = width * 50;
        bool                              backlight_on_value = true;
        bool                              reset_value        = false;
        bool                              invert_colors      = true;
        int                               offset_x           = 0;
        int                               offset_y           = 0;
        bool                              mirror_x           = true;
        bool                              mirror_y           = true;
        auto                              rotation           = espp::DisplayRotation::LANDSCAPE;
        //! [box_config example]
#elif CONFIG_SMARTKNOB_HA
        //! [smartknob_config example]
        static constexpr std::string_view dev_kit            = "Smartknob-HA";
        int                               clock_speed        = 80 * 1000 * 1000;
        gpio_num_t                        mosi               = GPIO_NUM_6;
        gpio_num_t                        sclk               = GPIO_NUM_5;
        gpio_num_t                        spics              = GPIO_NUM_15;
        gpio_num_t                        reset              = GPIO_NUM_4;
        gpio_num_t                        dc_pin             = (gpio_num_t) DC_PIN_NUM;
        gpio_num_t                        backlight          = GPIO_NUM_7;
        size_t                            width              = 240;
        size_t                            height             = 240;
        size_t                            pixel_buffer_size  = width * 50;
        bool                              backlight_on_value = true;
        bool                              reset_value        = false;
        bool                              invert_colors      = true;
        int                               offset_x           = 0;
        int                               offset_y           = 0;
        bool                              mirror_x           = true;
        bool                              mirror_y           = true;
        auto                              rotation           = espp::DisplayRotation::LANDSCAPE;
        //! [smartknob_config example]
#elif CONFIG_T_ENCODER_PRO
        static constexpr std::string_view dev_kit           = "Lilygo T-Encoder-Pro";
        int                               clock_speed       = 80 * 1000 * 1000;
        constexpr gpio_num_t              mosi              = GPIO_NUM_11;
        constexpr gpio_num_t              miso              = GPIO_NUM_13;
        constexpr gpio_num_t              data2             = GPIO_NUM_7;
        constexpr gpio_num_t              data3             = GPIO_NUM_14;
        constexpr gpio_num_t              sclk              = GPIO_NUM_12;
        constexpr gpio_num_t              spics             = GPIO_NUM_10;
        constexpr gpio_num_t              reset             = GPIO_NUM_4;
        constexpr gpio_num_t              enable            = GPIO_NUM_3;
        constexpr size_t                  width             = 390;
        constexpr size_t                  height            = 390;
        constexpr size_t                  pixel_buffer_size = width * height / 10;

        static lv_color_t EXT_RAM_BSS_ATTR frameBuffer_0[pixel_buffer_size];
        static lv_color_t EXT_RAM_BSS_ATTR frameBuffer_1[pixel_buffer_size];
        bool                               reset_value   = false;
        bool                               invert_colors = false;
        int                                offset_x      = 0;
        int                                offset_y      = 0;
        bool                               mirror_x      = false;
        bool                               mirror_y      = false;
        auto                               rotation      = espp::DisplayRotation::LANDSCAPE;
#endif

        fmt::print("Starting display_drivers example for {}\n", dev_kit);
        {
#ifdef CONFIG_T_ENCODER_PRO
            constexpr gpio_config_t o_conf{.pin_bit_mask = (1ULL << enable),
                                           .mode         = GPIO_MODE_OUTPUT,
                                           .pull_up_en   = GPIO_PULLUP_DISABLE,
                                           .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                           .intr_type    = GPIO_INTR_DISABLE};
            ESP_ERROR_CHECK(gpio_config(&o_conf));

            gpio_set_level(enable, 1);
#endif
            //! [display_drivers example]
            // create the spi host
            spi_bus_config_t buscfg;
            memset(&buscfg, 0, sizeof(buscfg));
            buscfg.mosi_io_num = mosi;
#ifndef CONFIG_T_ENCODER_PRO
            buscfg.miso_io_num   = -1;
            buscfg.quadwp_io_num = -1;
            buscfg.quadhd_io_num = -1;
#else
            buscfg.miso_io_num  = miso;
            buscfg.data2_io_num = data2;
            buscfg.data3_io_num = data3;
#endif

            buscfg.sclk_io_num     = sclk;
            buscfg.max_transfer_sz = 32768;
            // buscfg.max_transfer_sz = (int) (pixel_buffer_size * sizeof(lv_color_t));
            // create the spi device
            spi_device_interface_config_t devcfg;
            memset(&devcfg, 0, sizeof(devcfg));
            devcfg.mode           = 0;
            devcfg.clock_speed_hz = clock_speed;
            devcfg.input_delay_ns = 0;
            devcfg.spics_io_num   = spics;
            devcfg.queue_size     = spi_queue_size;
#ifdef CONFIG_T_ENCODER_PRO
            devcfg.flags        = SPI_DEVICE_HALFDUPLEX;
            devcfg.command_bits = 8;
            devcfg.address_bits = 24;
#endif
            // devcfg.pre_cb = lcd_spi_pre_transfer_callback;
            devcfg.post_cb = lcd_spi_post_transfer_callback;
            esp_err_t ret;
            // Initialize the SPI bus
            ret = spi_bus_initialize(spi_num, &buscfg, SPI_DMA_CH_AUTO);
            ESP_ERROR_CHECK(ret);
            // Attach the LCD to the SPI bus
            ret = spi_bus_add_device(spi_num, &devcfg, &spi);
            ESP_ERROR_CHECK(ret);

            // initialize the controller
            DisplayDriver::initialize(espp::display_drivers::Config{
                    .write_command  = write_command,
                    .lcd_send_lines = lcd_send_lines,
                    .reset_pin      = reset,
#ifndef CONFIG_T_ENCODER_PRO
                    .data_command_pin   = dc_pin,
                    .backlight_pin      = backlight,
                    .backlight_on_value = backlight_on_value,
#endif
                    .reset_value = reset_value,
                    // .invert_colors = invert_colors,
                    // .offset_x      = offset_x,
                    // .offset_y      = offset_y,
                    // .mirror_x      = mirror_x,
                    // .mirror_y      = mirror_y,
            });
        }
        // initialize the display / lvgl
        auto display = std::make_shared<Display>(
                Display::AllocatingConfig{
                        // .vram0             = frameBuffer_0,
                        // .vram1             = frameBuffer_1,
                        .width             = width,
                        .height            = height,
                        .pixel_buffer_size = pixel_buffer_size,
                        .flush_callback    = DisplayDriver::flush,
                        .rotation_callback = DisplayDriver::rotate,
                        .rotation          = rotation});

        DisplayDriver::set_brightness((uint8_t) 255);

        // initialize the gui
        // Gui    gui({});

        lv_obj_t* column_;
        lv_obj_t* label_;
        lv_obj_t* meter_;

        auto disp    = lv_display_get_default();
        auto hor_res = lv_display_get_horizontal_resolution(disp);
        auto ver_res = lv_display_get_vertical_resolution(disp);

        // Create a container with COLUMN flex direction
        column_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(column_, hor_res, ver_res);
        lv_obj_set_flex_flow(column_, LV_FLEX_FLOW_COLUMN);

        label_ = lv_label_create(column_);
        lv_label_set_text(label_, "Hello world");

        meter_ = lv_bar_create(lv_screen_active());
        lv_obj_set_size(meter_, hor_res * 0.8f, 20);
        lv_obj_center(meter_);

        static lv_style_t style_indic;
        lv_style_init(&style_indic);
        lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
        lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_BLUE));
        lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_RED));
        lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_HOR);

        lv_obj_add_style(meter_, &style_indic, LV_PART_INDICATOR);

        xTaskCreatePinnedToCore(Gui::lvgl_task, "LVGL", 32678, NULL, 24, NULL, 0);

        size_t iterations = 0;
        while (true) {
            auto label = fmt::format("Iterations: {}", iterations);
            printf("Label: %s\n", label.c_str());
            // gui.set_label(label);
            // gui.set_meter(iterations % 100, false);
            std::string test = "Hello World " + std::to_string(iterations);
            lv_label_set_text(label_, test.c_str());
            lv_bar_set_value(meter_, iterations % 100, LV_ANIM_ON);
            iterations++;
            std::this_thread::sleep_for(100ms);
        }
        //! [display_drivers example]
        // and sleep
        std::this_thread::sleep_for(num_seconds_to_run * 1s);
    }

    fmt::print("Display Driver example complete!\n");

    while (true) {
        std::this_thread::sleep_for(1s);
    }
}
