idf_component_register(
SRCS 
    "main.c"
    "hardware/oled/oled.c" 
    "hardware/fingerprint_reader/fg_reader.c" 
    "hardware/input/keypad.c" "hardware/input/button.c" 
    "hardware/flash/src/fal.c" "hardware/flash/src/fal_rtt.c" "hardware/flash/src/fal_flash.c" "hardware/flash/src/fal_partition.c" "hardware/flash/src/fal_flash_esp32.c"
    "system/flashdb/src/fdb.c" "system/flashdb/src/fdb_file.c" "system/flashdb/src/fdb_kvdb.c" "system/flashdb/src/fdb_tsdb.c" "system/flashdb/src/fdb_utils.c"
    "assest/font/uni_font.c" "assest/font/font24.c" "assest/font/font16.c"
    "ui/activity.c" "ui/view.c" "ui/widget.c" "ui/app.c"

INCLUDE_DIRS
    "." "hardware/oled" "hardware/fingerprint_reader" "hardware/input" "hardware/flash/inc" "system/flashdb/inc" "ui"
    "assest/font"

REQUIRES 
        esp_partition
        esp_timer
        esp_driver_gpio
        esp_driver_uart
        esp_driver_i2c
)
