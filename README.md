# Firmware IDF

## Using LVGL

The `lvgl_esp32_drivers` submodule in `components/` has to be modified to work properly.

1. Change the sixth parameter on [line 104 of `lvgl_helpers.c`](https://github.com/lvgl/lvgl_esp32_drivers/blob/master/lvgl_helpers.c#L104) to `SPI_DMA_CH_AUTO`:

    ```cpp
    lvgl_spi_driver_init(TFT_SPI_HOST,
        DISP_SPI_MISO, DISP_SPI_MOSI, DISP_SPI_CLK,
        SPI_BUS_MAX_TRANSFER_SZ, SPI_DMA_CH_AUTO,
        DISP_SPI_IO2, DISP_SPI_IO3);
    ```
2.  Add the following to [lvgl_helpers.h](https://github.com/lvgl/lvgl_esp32_drivers/blob/master/lvgl_helpers.h#L25):
    ```cpp
    #define LV_HOR_RES_MAX 240
    #define LV_VER_RES_MAX 240
    #define SPI_HOST_MAX 3
    ```