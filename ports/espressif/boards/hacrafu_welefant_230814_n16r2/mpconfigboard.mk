USB_VID = 0x303A
USB_PID = 0x7003
USB_PRODUCT = "Welefant"
USB_MANUFACTURER = "HaCraFu"

IDF_TARGET = esp32s3

CIRCUITPY_ESP_FLASH_MODE = dio
CIRCUITPY_ESP_FLASH_FREQ = 80m
CIRCUITPY_ESP_FLASH_SIZE = 16MB
#CIRCUITPY_AUDIOBUSIO_PDMIN = 1

CIRCUITPY_ESPCAMERA = 0

# Include these Python libraries in firmware.
FROZEN_MPY_DIRS += $(TOP)/frozen/Adafruit_CircuitPython_asyncio
FROZEN_MPY_DIRS += $(TOP)/frozen/Adafruit_CircuitPython_NeoPixel
FROZEN_MPY_DIRS += $(TOP)/frozen/Adafruit_CircuitPython_LED_Animation
FROZEN_MPY_DIRS += $(TOP)/frozen/Adafruit_CircuitPython_framebuf
FROZEN_MPY_DIRS += $(TOP)/frozen/Adafruit_CircuitPython_Pixel_Framebuf
FROZEN_MPY_DIRS += $(TOP)/frozen/Adafruit_CircuitPython_SimpleMath
FROZEN_MPY_DIRS += $(TOP)/frozen/Adafruit_CircuitPython_Ticks
