/*
m5_paper_s3_pins.hpp
M5 Paper S3 GPIO Pin Definitions

Official pinout from M5Stack M5Paper S3 hardware specifications:
- Display: EPD_ED047TC1 e-ink controller
- Touch: GT911 capacitive touch
- RTC: BM8563
- IMU: BMI270
- Power: AXP2101 via I2C

This code is released under the GNU Lesser General Public License v3.0:
https://www.gnu.org/licenses/lgpl-3.0.en.html
*/

#pragma once

#include "driver/gpio.h"

// ========== DISPLAY AND EPD INTERFACE ==========
// EPD_ED047TC1 Parallel Interface (8-bit data + control)
namespace M5Paper3Pins {

  // Display Data Bus (DB0-DB7)
  static constexpr gpio_num_t EPD_DB0   = GPIO_NUM_6;
  static constexpr gpio_num_t EPD_DB1   = GPIO_NUM_14;
  static constexpr gpio_num_t EPD_DB2   = GPIO_NUM_7;
  static constexpr gpio_num_t EPD_DB3   = GPIO_NUM_12;
  static constexpr gpio_num_t EPD_DB4   = GPIO_NUM_9;
  static constexpr gpio_num_t EPD_DB5   = GPIO_NUM_11;
  static constexpr gpio_num_t EPD_DB6   = GPIO_NUM_8;
  static constexpr gpio_num_t EPD_DB7   = GPIO_NUM_10;

  // Display Control Signals
  static constexpr gpio_num_t EPD_XSTL  = GPIO_NUM_13;  // Strobe Pin
  static constexpr gpio_num_t EPD_XLE   = GPIO_NUM_15;  // Latch Enable
  static constexpr gpio_num_t EPD_SPV   = GPIO_NUM_17;  // Start Pulse
  static constexpr gpio_num_t EPD_CKV   = GPIO_NUM_18;  // Clock Signal
  static constexpr gpio_num_t EPD_PWR   = GPIO_NUM_45;  // Power Control (OE - Output Enable)

  // ========== TOUCH SCREEN (GT911) ==========
  // Shared I2C0 Bus (41/42)
  static constexpr gpio_num_t TOUCH_SDA = GPIO_NUM_41;
  static constexpr gpio_num_t TOUCH_SCL = GPIO_NUM_42;
  static constexpr gpio_num_t TOUCH_INT = GPIO_NUM_48;  // Interrupt pin
  static constexpr uint16_t   TOUCH_ADDR = 0x14;        // I2C address (0x28 >> 1)

  // ========== RTC (BM8563) ==========
  // Shared I2C0 Bus with GT911 and BMI270
  static constexpr gpio_num_t RTC_SDA   = GPIO_NUM_41;
  static constexpr gpio_num_t RTC_SCL   = GPIO_NUM_42;
  static constexpr uint16_t   RTC_ADDR  = 0x51;         // I2C address

  // ========== IMU (BMI270) ==========
  // Shared I2C0 Bus
  static constexpr gpio_num_t IMU_SDA   = GPIO_NUM_41;
  static constexpr gpio_num_t IMU_SCL   = GPIO_NUM_42;
  static constexpr uint16_t   IMU_ADDR  = 0x68;         // I2C address (official specs)

  // I2C0 Configuration
  static constexpr int I2C_PORT = 0;
  static constexpr uint32_t I2C_FREQ = 100000;  // 100kHz

  // ========== microSD CARD ==========
  static constexpr gpio_num_t SD_CS   = GPIO_NUM_47;
  static constexpr gpio_num_t SD_SCK  = GPIO_NUM_39;
  static constexpr gpio_num_t SD_MOSI = GPIO_NUM_38;
  static constexpr gpio_num_t SD_MISO = GPIO_NUM_40;

  // ========== POWER MANAGEMENT (PMS150G) ==========
  // PMS150G: GPIO-based power control (not I2C)
  // Controls power-on/off, program download mode
  // TODO: Confirm exact GPIO pins for PMS150G control based on schematic
  // Typical control signals: POWER_ON, POWER_OFF, PROGRAM_MODE
  static constexpr gpio_num_t POWER_ENABLE = GPIO_NUM_44;     // Power enable/control (placeholder)
  static constexpr gpio_num_t POWER_HOLD   = GPIO_NUM_43;     // Hold power state (placeholder)

  // ========== BATTERY ==========
  // 3.7V @ 1800mAh lithium battery
  // Charging: LGS4056H chip @ DC 5V @ 331.5mA
  // Battery voltage detection via ADC1 channel 3 (GPIO 36)
  static constexpr gpio_num_t BATTERY_ADC_PIN = GPIO_NUM_36;
  static constexpr int BATTERY_ADC_CHANNEL = 3;   // ADC1_CHANNEL_3
  static constexpr int BATTERY_ADC_UNIT = 0;      // ADC1 (ADC_UNIT_1)

  // ========== USB POWER DETECTION ==========
  static constexpr gpio_num_t USB_DETECT = GPIO_NUM_5;

  // ========== BUZZER (Piezo Speaker) ==========
  static constexpr gpio_num_t BUZZER_PWM = GPIO_NUM_3;

  // ========== I2C DEVICE LIST ==========
  // All I2C devices on I2C0 (GPIO41=SDA, GPIO42=SCL):
  // - GT911 Touch (0x14)
  // - BM8563 RTC (0x51)
  // - BMI270 IMU (0x68)
  // 
  // ========== PHYSICAL BUTTON ==========
  // 1x physical button for device control/power/reset
  // Connected to GPIO 0 with pull-up (standard ESP32-S3 power button)
  // Note: This is a strapping pin; confirm in schematic
  static constexpr gpio_num_t BUTTON_GPIO = GPIO_NUM_0;       // Physical power button (press = GND)

}  // namespace M5Paper3Pins
