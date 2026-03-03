/*
m5_paper_s3_power.cpp
M5 Paper S3 Power Management Implementation

This code is released under the GNU Lesser General Public License v3.0
*/

#include "m5_paper_s3_power.hpp"
#include "logging.hpp"

static constexpr char const * TAG = "M5Paper3Power";

M5Paper3PowerManager::M5Paper3PowerManager()
  : initialized(false), i2c_port(I2C_NUM_0)
{
  LOG_I("M5Paper3PowerManager constructor");
}

M5Paper3PowerManager::~M5Paper3PowerManager()
{
  LOG_I("M5Paper3PowerManager destructor");
}

bool M5Paper3PowerManager::init(i2c_port_t i2c_port_in)
{
  LOG_I("M5Paper3PowerManager::init()");
  i2c_port = i2c_port_in;

  // Verify AXP2101 is accessible on I2C
  uint8_t chip_id;
  if (!read_register(0x03, chip_id)) {  // Chip ID register
    LOG_E("Failed to read AXP2101 chip ID - device may not be present");
    return false;
  }
  
  LOG_I("AXP2101 chip ID: 0x%02x", chip_id);

  // Initialize power management defaults
  // Enable battery monitoring
  
  initialized = true;
  LOG_I("M5Paper3PowerManager initialized successfully");
  return true;
}

float M5Paper3PowerManager::get_battery_voltage()
{
  LOG_D("M5Paper3PowerManager::get_battery_voltage()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return 0.0f;
  }

  uint8_t vbat_data[2];
  if (!read_registers(REG_VBAT_H, vbat_data, 2)) {
    LOG_E("Failed to read battery voltage registers");
    return 0.0f;
  }

  float voltage = 0.0f;
  parse_voltage(vbat_data[0], vbat_data[1], voltage);
  
  LOG_D("Battery voltage: %.3f V", voltage);
  return voltage;
}

int M5Paper3PowerManager::get_battery_percentage()
{
  LOG_D("M5Paper3PowerManager::get_battery_percentage()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return 0;
  }

  // For a lithium battery, map 2.5V (0%) to 4.2V (100%)
  float voltage = get_battery_voltage();
  
  if (voltage <= 2.5f) return 0;
  if (voltage >= 4.2f) return 100;
  
  // Linear interpolation: (voltage - 2.5) / (4.2 - 2.5) * 100
  int percentage = static_cast<int>((voltage - 2.5f) / 1.7f * 100.0f);
  
  if (percentage > 100) percentage = 100;
  if (percentage < 0) percentage = 0;
  
  LOG_D("Battery percentage: %d%%", percentage);
  return percentage;
}

bool M5Paper3PowerManager::is_charging()
{
  LOG_D("M5Paper3PowerManager::is_charging()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  uint8_t status;
  if (!read_register(REG_POWER_STATUS, status)) {
    LOG_E("Failed to read power status");
    return false;
  }

  return (status & BIT_CHARGING) != 0;
}

bool M5Paper3PowerManager::is_power_connected()
{
  LOG_D("M5Paper3PowerManager::is_power_connected()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  uint8_t status;
  if (!read_register(REG_INPUT_STATUS, status)) {
    LOG_E("Failed to read input status");
    return false;
  }

  return (status & (BIT_USB_PRESENT | BIT_AC_PRESENT)) != 0;
}

bool M5Paper3PowerManager::enter_deep_sleep(uint32_t duration_ms)
{
  LOG_I("M5Paper3PowerManager::enter_deep_sleep(%u ms)", duration_ms);
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // Configure timer-based wakeup if duration specified
  if (duration_ms > 0) {
    if (!configure_timer_wakeup(duration_ms)) {
      LOG_E("Failed to configure timer wakeup");
      return false;
    }
  }

  // Enable deep sleep: set bit 6 in SLEEP_CONFIG (0x31)
  uint8_t sleep_config;
  if (!read_register(REG_SLEEP_CONFIG, sleep_config)) {
    LOG_E("Failed to read sleep config");
    return false;
  }

  sleep_config |= 0x40;  // Enable deep sleep
  if (!write_register(REG_SLEEP_CONFIG, sleep_config)) {
    LOG_E("Failed to enable deep sleep");
    return false;
  }

  LOG_I("Deep sleep enabled");
  return true;
}

bool M5Paper3PowerManager::exit_deep_sleep()
{
  LOG_I("M5Paper3PowerManager::exit_deep_sleep()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // Disable deep sleep: clear bit 6 in SLEEP_CONFIG (0x31)
  uint8_t sleep_config;
  if (!read_register(REG_SLEEP_CONFIG, sleep_config)) {
    LOG_E("Failed to read sleep config");
    return false;
  }

  sleep_config &= ~0x40;  // Disable deep sleep
  if (!write_register(REG_SLEEP_CONFIG, sleep_config)) {
    LOG_E("Failed to disable deep sleep");
    return false;
  }

  LOG_I("Deep sleep disabled");
  return true;
}

bool M5Paper3PowerManager::set_gpio0_power_output(bool enable)
{
  LOG_I("M5Paper3PowerManager::set_gpio0_power_output(%s)", enable ? "ON" : "OFF");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // Set GPIO0 control register (0x90)
  // Bit 1: Output level (1=HIGH, 0=LOW)
  // Bit 0: Function select (1=GPIO, 0=ADC input)
  uint8_t gpio_ctrl = enable ? 0x03 : 0x02;  // Enable: GPIO output HIGH, Disable: GPIO output LOW
  
  if (!write_register(REG_GPIO0_CTRL, gpio_ctrl)) {
    LOG_E("Failed to set GPIO0 control");
    return false;
  }

  LOG_I("GPIO0 power output %s", enable ? "enabled" : "disabled");
  return true;
}

float M5Paper3PowerManager::get_internal_temperature()
{
  LOG_D("M5Paper3PowerManager::get_internal_temperature()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return 0.0f;
  }

  uint8_t temp_data[2];
  if (!read_registers(REG_TEMP_H, temp_data, 2)) {
    LOG_E("Failed to read internal temperature registers");
    return 0.0f;
  }

  float temperature = 0.0f;
  parse_temperature(temp_data[0], temp_data[1], temperature);
  
  LOG_D("Internal temperature: %.1f C", temperature);
  return temperature;
}

float M5Paper3PowerManager::get_battery_temperature()
{
  LOG_D("M5Paper3PowerManager::get_battery_temperature()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return 0.0f;
  }

  // Battery temperature uses same registers as internal temperature on AXP2101
  // Read from REG_TEMP_H and REG_TEMP_L
  uint8_t temp_data[2];
  if (!read_registers(REG_TEMP_H, temp_data, 2)) {
    LOG_E("Failed to read battery temperature registers");
    return 0.0f;
  }

  float temperature = 0.0f;
  parse_temperature(temp_data[0], temp_data[1], temperature);
  
  LOG_D("Battery temperature: %.1f C", temperature);
  return temperature;
}

bool M5Paper3PowerManager::configure_button_wakeup()
{
  LOG_I("M5Paper3PowerManager::configure_button_wakeup()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // Configure power button as wakeup source in SLEEP_CONFIG (0x31)
  // Bit 7: Power button wakeup enable
  uint8_t sleep_config;
  if (!read_register(REG_SLEEP_CONFIG, sleep_config)) {
    LOG_E("Failed to read sleep config");
    return false;
  }

  sleep_config |= 0x80;  // Enable power button wakeup
  if (!write_register(REG_SLEEP_CONFIG, sleep_config)) {
    LOG_E("Failed to configure button wakeup");
    return false;
  }

  LOG_I("Power button wakeup configured");
  return true;
}

bool M5Paper3PowerManager::configure_timer_wakeup(uint32_t duration_ms)
{
  LOG_I("M5Paper3PowerManager::configure_timer_wakeup(%u ms)", duration_ms);
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // Timer wakeup configuration in SLEEP_CONFIG (0x31)
  // Bits 5-4: Timer select (00=none, 01=8s, 10=16s, 11=32s)
  // Bit 6: Deep sleep enable
  // Bit 7: Button wakeup enable
  
  // Map duration_ms to timer value
  uint8_t timer_val = 0;  // No timer by default
  
  if (duration_ms >= 32000) {
    timer_val = 0x30;  // 32 seconds
  } else if (duration_ms >= 16000) {
    timer_val = 0x20;  // 16 seconds
  } else if (duration_ms >= 8000) {
    timer_val = 0x10;  // 8 seconds
  } else if (duration_ms > 0) {
    // For durations < 8 seconds, use 8 second timer
    timer_val = 0x10;
  }

  // Read current config and update timer bits
  uint8_t sleep_config;
  if (!read_register(REG_SLEEP_CONFIG, sleep_config)) {
    LOG_E("Failed to read sleep config");
    return false;
  }

  // Clear timer bits (5-4) and set new value
  sleep_config = (sleep_config & 0xCF) | timer_val;
  
  if (!write_register(REG_SLEEP_CONFIG, sleep_config)) {
    LOG_E("Failed to configure timer wakeup");
    return false;
  }

  LOG_I("Timer wakeup configured: %u ms", duration_ms);
  return true;
}

bool M5Paper3PowerManager::write_register(uint8_t address, uint8_t data)
{
  LOG_D("M5Paper3PowerManager::write_register(0x%02x, 0x%02x)", address, data);
  
  uint8_t write_buffer[2] = { address, data };
  esp_err_t err = i2c_master_write_to_device(i2c_port, AXP2101_I2C_ADDR, write_buffer, 2, 1000 / portTICK_PERIOD_MS);
  
  if (err != ESP_OK) {
    LOG_E("I2C write failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

bool M5Paper3PowerManager::read_register(uint8_t address, uint8_t & data)
{
  LOG_D("M5Paper3PowerManager::read_register(0x%02x)", address);
  
  esp_err_t err = i2c_master_write_read_device(i2c_port, AXP2101_I2C_ADDR, &address, 1, &data, 1, 1000 / portTICK_PERIOD_MS);
  
  if (err != ESP_OK) {
    LOG_E("I2C read failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

bool M5Paper3PowerManager::read_registers(uint8_t start_addr, uint8_t * data, size_t length)
{
  LOG_D("M5Paper3PowerManager::read_registers(0x%02x, length:%zu)", start_addr, length);
  
  esp_err_t err = i2c_master_write_read_device(i2c_port, AXP2101_I2C_ADDR, &start_addr, 1, data, length, 1000 / portTICK_PERIOD_MS);
  
  if (err != ESP_OK) {
    LOG_E("I2C burst read failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

void M5Paper3PowerManager::parse_voltage(uint8_t msb, uint8_t lsb, float & voltage)
{
  // AXP2101 battery voltage conversion
  // VBAT_H (0x24): bits 7-0 = 11 high bits
  // VBAT_L (0x25): bits 7-4 = 4 low bits
  // Voltage = (11 high bits + 4 low bits/16) * 1.1mV + 2.6V
  
  uint16_t raw_voltage = ((uint16_t)msb << 4) | ((lsb >> 4) & 0x0F);
  voltage = (raw_voltage * 1.1f) / 1000.0f + 2.6f;
}

void M5Paper3PowerManager::parse_temperature(uint8_t msb, uint8_t lsb, float & temperature)
{
  // AXP2101 temperature conversion
  // TEMP_H (0x56): bits 7-0 = temperature integer part
  // TEMP_L (0x57): bits 7-4 = temperature fractional part (x 0.0625°C)
  // Range: -30°C to +85°C
  
  int8_t temp_int = static_cast<int8_t>(msb);
  float temp_frac = ((lsb >> 4) & 0x0F) * 0.0625f;
  temperature = temp_int + temp_frac;
}
