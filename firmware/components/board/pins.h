// T-Deck v1 pinout. Source: Xinyuan-LilyGO/T-Deck, see docs/hardware.md.
#pragma once

#define PIN_POWERON 10  // must be high before any peripheral is used

#define PIN_I2C_SDA 18
#define PIN_I2C_SCL 8

#define PIN_SPI_MOSI 41
#define PIN_SPI_MISO 38
#define PIN_SPI_SCK 40
#define PIN_TFT_CS 12
#define PIN_TFT_DC 11
#define PIN_TFT_BACKLIGHT 42
#define PIN_SDCARD_CS 39  // shares the SPI bus
#define PIN_RADIO_CS 9    // shares the SPI bus

#define PIN_TOUCH_INT 16

#define PIN_TRACKBALL_RIGHT 2
#define PIN_TRACKBALL_UP 3
#define PIN_TRACKBALL_LEFT 1
#define PIN_TRACKBALL_DOWN 15
#define PIN_TRACKBALL_CLICK 0  // also the boot strapping pin

#define PIN_BATTERY_ADC 4
#define BATTERY_DIVIDER 2

#define KEYBOARD_I2C_ADDR 0x55
#define KEYBOARD_CMD_BRIGHTNESS 0x01
