/*
m5_paper_s3_power.hpp
M5 Paper S3 Power Management

AXP2101 Power Management IC driver for M5 Paper S3
I2C communication (address: 0x34)

Features:
- Battery level monitoring
- Charging status
- Power input detection
- Deep sleep configuration
- Temperature monitoring

This code is released under the GNU Lesser General Public License v3.0
*/

#pragma once

#include <cinttypes>
#include "driver/i2c.h"
#include "esp_log.h"

/**
 * @brief AXP2101 Power Management IC driver
 * 
 * Manages power-related functionality of M5 Paper S3
 */

class M5Paper3PowerManager
{
  public:
    M5Paper3PowerManager();
    ~M5Paper3PowerManager();

    bool init(i2c_port_t i2c_port);
    bool is_initialized() { return initialized; }

    // Battery status
    float get_battery_voltage();      // Returns voltage in volts (0.0-5.0V)
    int get_battery_percentage();     // Returns 0-100%
    bool is_charging();               // Returns true if charging
    bool is_power_connected();        // Returns true if USB/external power connected

    // Power modes
    bool enter_deep_sleep(uint32_t duration_ms);
    bool exit_deep_sleep();

    // GPIO control
    bool set_gpio0_power_output(bool enable);  // Controls USB power output

    // Temperature monitoring
    float get_internal_temperature();  // Returns temperature in Celsius
    float get_battery_temperature();   // Returns battery temperature

    // Wakeup configuration
    bool configure_button_wakeup();
    bool configure_timer_wakeup(uint32_t duration_ms);

  private:
    static constexpr char const * TAG = "M5Paper3Power";
    static constexpr uint8_t AXP2101_I2C_ADDR = 0x34;

    bool initialized;
    i2c_port_t i2c_port;

    // Register operations
    bool write_register(uint8_t address, uint8_t data);
    bool read_register(uint8_t address, uint8_t & data);
    bool read_registers(uint8_t start_addr, uint8_t * data, size_t length);

    // Register addresses (AXP2101)
    static constexpr uint8_t REG_INPUT_STATUS     = 0x00;
    static constexpr uint8_t REG_POWER_STATUS     = 0x01;
    static constexpr uint8_t REG_VBAT_H           = 0x24;
    static constexpr uint8_t REG_VBAT_L           = 0x25;
    static constexpr uint8_t REG_CHARGE_STATUS    = 0x09;
    static constexpr uint8_t REG_TEMP_H           = 0x56;
    static constexpr uint8_t REG_TEMP_L           = 0x57;
    static constexpr uint8_t REG_SLEEP_CONFIG     = 0x31;
    static constexpr uint8_t REG_GPIO0_CTRL       = 0x90;

    // Bit masks
    static constexpr uint8_t BIT_USB_PRESENT      = 0x10;
    static constexpr uint8_t BIT_AC_PRESENT       = 0x08;
    static constexpr uint8_t BIT_CHARGING         = 0x04;
    static constexpr uint8_t BIT_BATTERY_EXIST    = 0x01;

    // Helper methods
    void parse_voltage(uint8_t msb, uint8_t lsb, float & voltage);
    void parse_temperature(uint8_t msb, uint8_t lsb, float & temperature);
};
