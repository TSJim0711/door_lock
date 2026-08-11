idf_component_register(
SRCS 
    "main.c"
    "hardware/oled/oled.c" 
    "hardware/fingerprint_reader/fg_reader.c" 
    "assest/font/uni_font.c" "assest/font/font24.c" "assest/font/font16.c"

INCLUDE_DIRS
    "." "hardware/oled" "hardware/fingerprint_reader" 
    "assest/font"

)
