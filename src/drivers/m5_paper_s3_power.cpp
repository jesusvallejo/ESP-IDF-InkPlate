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

  // TODO: Check charging status bit
  return false;
}

bool M5Paper3PowerManager::is_power_connected()
{
  LOG_D("M5Paper3PowerManager::is_power_connected()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // TODO: Check USB/AC power input status
  return false;
}

bool M5Paper3PowerManager::enter_deep_sleep(uint32_t duration_ms)
{
  LOG_I("M5Paper3PowerManager::enter_deep_sleep(%u ms)", duration_ms);
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // TODO: Configure timer wakeup
  // TODO: Enable deep sleep mode
  return true;
}

bool M5Paper3PowerManager::exit_deep_sleep()
{
  LOG_I("M5Paper3PowerManager::exit_deep_sleep()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // TODO: Disable deep sleep mode
  return true;
}

bool M5Paper3PowerManager::set_gpio0_power_output(bool enable)
{
  LOG_I("M5Paper3PowerManager::set_gpio0_power_output(%s)", enable ? "ON" : "OFF");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // TODO: Set GPIO0 control register for USB power output
  return true;
}

float M5Paper3PowerManager::get_internal_temperature()
{
  LOG_D("M5Paper3PowerManager::get_internal_temperature()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return 0.0f;
  }

  // TODO: Read internal temperature register
  return 0.0f;
}

float M5Paper3PowerManager::get_battery_temperature()
{
  LOG_D("M5Paper3PowerManager::get_battery_temperature()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return 0.0f;
  }

  // TODO: Read battery temperature sensor
  return 0.0f;
}

bool M5Paper3PowerManager::configure_button_wakeup()
{
  LOG_I("M5Paper3PowerManager::configure_button_wakeup()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // TODO: Configure power button as wakeup source
  return true;
}

bool M5Paper3PowerManager::configure_timer_wakeup(uint32_t duration_ms)
{
  LOG_I("M5Paper3PowerManager::configure_timer_wakeup(%u ms)", duration_ms);
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // TODO: Configure timer-based wakeup
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
  // TODO: Convert MSB/LSB to temperature value
  // AXP2101 specific conversion formula
  temperature = 0.0f;
}
