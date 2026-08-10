idf_component_register(
SRCS 
    "main.c"
    "device/oled/oled.c" 
    "device/fingerprint_reader/fg_reader.c" 
    "assest/font/uni_font.c" "assest/font/font24.c" "assest/font/font16.c"

INCLUDE_DIRS
    "." "device/oled" "device/fingerprint_reader"
    "assest/font"

)
