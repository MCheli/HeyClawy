set(BOARD_REQUIRES
    driver
    esp_adc
    esp_lcd
    led_strip
    esp_timer
    lvgl
    esp_lvgl_port
)

set(BOARD_PRIV_REQUIRES
    esp_driver_i2s
    esp_driver_i2c
    esp_driver_spi
    esp_driver_gpio
    esp_driver_ledc
    esp_driver_uart
)

# ESP32-S3 boards need display + touch + IO expander drivers
if(IDF_TARGET STREQUAL "esp32s3")
    list(APPEND BOARD_REQUIRES
        esp_lcd_spd2010
        esp_lcd_touch_spd2010
        esp_lcd_touch_gt911
        esp_lcd_ili9341
    )
    list(APPEND BOARD_PRIV_REQUIRES
        esp_io_expander
        esp_io_expander_tca95xx_16bit
    )
endif()

idf_component_register(
    SRCS "board.c"
    INCLUDE_DIRS "include"
    REQUIRES ${BOARD_REQUIRES}
    PRIV_REQUIRES ${BOARD_PRIV_REQUIRES}
)
