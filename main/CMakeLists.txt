idf_component_register(
SRCS 
    "main.c"
    "hardware/oled/oled.c" 
    "hardware/fingerprint_reader/fg_reader.c" 
    "hardware/input/keypad.c" "hardware/input/button.c" 
    "assest/font/uni_font.c" "assest/font/font24.c" "assest/font/font16.c"
    "ui/activity.c" "ui/view.c" "ui/widget.c" "ui/app.c"

INCLUDE_DIRS
    "." "hardware/oled" "hardware/fingerprint_reader" "hardware/input" "ui"
    "assest/font"
)
