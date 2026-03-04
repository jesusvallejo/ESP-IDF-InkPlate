/*
m5_paper_s3_power.hpp
M5 Paper S3 Power Management

PMS150G Power Management IC for M5 Paper S3
GPIO-based control for power on/off and program download

Power Information:
- Battery: 3.7V @ 1800mAh lithium
- Charging: LGS4056H chip @ DC 5V @ 331.5mA
- Battery Interface: HY1.25-2P connector
- Low power mode: 9.28µA
- Standby mode: 949.58µA  
- Operating: ~154mA

Note: PMS150G is a GPIO-based power control chip, not I2C-based.
No register-based access; power control is via GPIO signals.
Battery voltage monitoring requires ADC (if circuit connected).

This code is released under the GNU Lesser General Public License v3.0
*/

#pragma once

#if defined(M5_PAPER_S3)

#include <cinttypes>
#include <ctime>
#include "driver/gpio.h"
#include "esp_log.h"

class BM8563RTC;  // Forward declaration

/**
 * @brief PMS150G Power Management and Battery monitoring with RTC wakeup
 * 
 * Manages power-related functionality of M5 Paper S3:
 * - Power control via GPIO (PMS150G chip)
 * - Battery voltage monitoring (if ADC channel available)
 * - RTC-based alarm wakeup from deep sleep
 */

class M5Paper3PowerManager
{
  public:
    M5Paper3PowerManager();
    ~M5Paper3PowerManager();

    bool init();
    bool is_initialized() { return initialized; }

    // Battery status  
    float get_battery_voltage();      // Returns voltage in volts via ADC (0.0-5.0V)
    int get_battery_percentage();     // Estimates percentage based on voltage (0-100%)
    bool is_charging();               // Check if USB power is connected
    bool is_power_connected();        // Check if external power is present

    // Power control
    bool power_off();                 // Trigger power-off sequence
    bool set_power_button_wakeup();   // Configure 1x button for wakeup (physical button on GPIO TBD)

    // RTC-based wakeup (via BM8563 alarm)
    bool setup_rtc();                 // Initialize BM8563 RTC
    bool set_rtc_wakeup(time_t wakeup_time);  // Set specific time to wake up
    bool set_rtc_timer_wakeup(uint8_t seconds);  // Wake up after N seconds via RTC timer
    bool clear_rtc_alarm();           // Clear RTC alarm flag
    bool is_rtc_ready();              // Check if RTC initialized successfully

  private:
    static constexpr char const * TAG = "M5Paper3Power";
    
    bool initialized;
    BM8563RTC * rtc;               // RTC instance for alarm wakeup
    bool rtc_initialized;          // Track if RTC init was successful

    // GPIO-based power control (PMS150G) 
    // - GPIO 44: POWER_ENABLE (HIGH=enabled, LOW=disabled)
    // - GPIO 43: POWER_HOLD (HIGH=hold power, LOW=release)
    bool init_pms150g();
    
    // ADC-based battery monitoring
    bool init_battery_adc();
    float read_battery_voltage_adc();
};

#endif  // M5_PAPER_S3
