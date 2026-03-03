/*
m5_paper_s3_power.cpp
M5 Paper S3 Power Management Implementation

PMS150G GPIO-based power control + ADC battery monitoring
RTC-based alarm wakeup via BM8563

Specifications:
- Battery: 3.7V @ 1800mAh lithium
- Charging: LGS4056H chip @ DC 5V @ 331.5mA
- Low power mode: 9.28µA
- Standby mode: 949.58µA  
- Operating: ~154mA
- RTC: BM8563 @ I2C0 (0x51) with alarm capability

This code is released under the GNU Lesser General Public License v3.0
*/

#include "m5_paper_s3_power.hpp"
#include "m5_paper_s3_rtc.hpp"
#include "logging.hpp"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

static constexpr char const * TAG = "M5Paper3Power";

M5Paper3PowerManager::M5Paper3PowerManager()
  : initialized(false), rtc(nullptr), rtc_initialized(false)
{
  LOG_I("M5Paper3PowerManager constructor");
}

M5Paper3PowerManager::~M5Paper3PowerManager()
{
  LOG_I("M5Paper3PowerManager destructor");
  if (rtc != nullptr) {
    delete rtc;
    rtc = nullptr;
  }
}

bool M5Paper3PowerManager::init()
{
  LOG_I("M5Paper3PowerManager::init()");
  
  // Initialize ADC for battery voltage monitoring
  if (!init_battery_adc()) {
    LOG_W("Battery ADC initialization failed - continuing without battery monitoring");
  }
  
  // Initialize RTC for wakeup capability
  if (!setup_rtc()) {
    LOG_W("RTC initialization failed - continuing without RTC wakeup");
  }
  
  // Initialize PMS150G power control via GPIO signals
  if (!init_pms150g()) {
    LOG_W("PMS150G initialization failed - continuing without GPIO power control");
  }
  
  initialized = true;
  LOG_I("M5Paper3PowerManager initialized successfully");
  return true;
}

bool M5Paper3PowerManager::init_battery_adc()
{
  LOG_I("M5Paper3PowerManager::init_battery_adc()");
  
  // Battery voltage on ADC1_CHANNEL_3 (GPIO 36)
  // Battery: 3.7V nominal (LiPo safe range: 3.0V-4.2V)
  // Voltage divider: Input 0-5V mapped to 0-3.3V for ADC
  // Ratio: Vbatt * R2/(R1+R2) = ADC_input
  // For 5V input max, need R1:R2 ratio of ~1:1 (typically 100k:100k)
  // Then: Vbatt = ADC_mV * (1+1) = ADC_mV * 2 / 1000 volts
  
  // Initialize ADC1 channel 3 (GPIO 36)
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);  // 0-3.3V range
  
  LOG_I("Battery ADC configured (ADC1, Channel 3, GPIO 36, 12-bit, 0-3.3V)");
  return true;
}

bool M5Paper3PowerManager::init_pms150g()
{
  LOG_I("M5Paper3PowerManager::init_pms150g()");
  
  // Configure PMS150G power management IC via GPIO signals
  // PMS150G: GPIO-based power control (not I2C)
  // Control signals: 
  //   - POWER_ENABLE (GPIO 44): HIGH = power output enabled, LOW = power output disabled
  //   - POWER_HOLD (GPIO 43): HIGH = hold power state, LOW = release power hold
  // Usage:
  //   - Power on: Set POWER_HOLD HIGH, POWER_ENABLE HIGH
  //   - Power off: Set POWER_HOLD LOW, POWER_ENABLE LOW
  //   - Program mode: pulse POWER_ENABLE to enter firmware download mode
  
  gpio_config_t gpio_cfg = {0};
  gpio_cfg.pin_bit_mask = (1ULL << 44) | (1ULL << 43);  // GPIO 44 (POWER_ENABLE), GPIO 43 (POWER_HOLD)
  gpio_cfg.mode = GPIO_MODE_OUTPUT;
  gpio_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&gpio_cfg);
  
  // Initialize to normal power state (both HIGH = powered on and held)
  gpio_set_level(GPIO_NUM_44, 1);  // POWER_ENABLE = HIGH (power output enabled)
  gpio_set_level(GPIO_NUM_43, 1);  // POWER_HOLD = HIGH (hold power state)
  
  LOG_I("PMS150G GPIO control initialized: POWER_ENABLE(GPIO44)=HIGH, POWER_HOLD(GPIO43)=HIGH");
  return true;
}

float M5Paper3PowerManager::read_battery_voltage_adc()
{
  LOG_D("Reading battery voltage via ADC");
  
  // Read ADC1_CHANNEL_3 (GPIO 36) and convert to battery voltage
  // ADC1 resolution: 12-bit (0-4095)
  // ADC attenuation: 11dB (0-3.3V range)
  // Voltage divider ratio: assumes 1:1 (100k:100k resistor network)
  
  // Read ADC value (multiple samples for averaging)
  int adc_raw = 0;
  for (int i = 0; i < 4; i++) {
    adc_raw += adc1_get_raw(ADC1_CHANNEL_3);
  }
  adc_raw /= 4;  // Average 4 samples
  
  // ADC full scale: 4095 = 3.3V (with 11dB attenuation)
  float adc_voltage = (adc_raw * 3.3f) / 4095.0f;
  
  // Account for 1:1 voltage divider (multiply by 2)
  float battery_voltage = adc_voltage * 2.0f;
  
  LOG_D("ADC raw: %d, ADC voltage: %.3fV, Battery: %.3fV", adc_raw, adc_voltage, battery_voltage);
  return battery_voltage;
}

float M5Paper3PowerManager::get_battery_voltage()
{
  LOG_D("M5Paper3PowerManager::get_battery_voltage()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return 0.0f;
  }

  float voltage = read_battery_voltage_adc();
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

  // For a lithium battery, map 3.0V (0%) to 4.2V (100%)
  float voltage = get_battery_voltage();
  
  if (voltage <= 3.0f) return 0;
  if (voltage >= 4.2f) return 100;
  
  // Linear interpolation between 3.0V and 4.2V
  int percentage = (int)((voltage - 3.0f) / (4.2f - 3.0f) * 100.0f);
  
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

  // Check GPIO 5 (USB_DETECT) to determine if USB power is connected
  // If USB is present at 5V, LGS4056H charging IC will charge battery
  // GPIO 5 = HIGH when USB power present
  
  if (!gpio_get_level(GPIO_NUM_5)) {
    LOG_D("External power detected - charging active");
    // Check if battery percentage < 100%
    return get_battery_percentage() < 100;
  }
  
  return false;
}

bool M5Paper3PowerManager::is_power_connected()
{
  LOG_D("M5Paper3PowerManager::is_power_connected()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // Check GPIO 5 (USB_DETECT) for 5V power presence
  // GPIO 5 = HIGH when external 5V power detected (USB or DC barrel jack)
  // GPIO 5 = LOW when running on battery only
  
  bool power_connected = gpio_get_level(GPIO_NUM_5) != 0;
  LOG_D("External power: %s", power_connected ? "YES" : "NO");
  
  return power_connected;
}

bool M5Paper3PowerManager::power_off()
{
  LOG_I("M5Paper3PowerManager::power_off()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // Trigger PMS150G power-off sequence via GPIO signals
  // PMS150G: Configure GPIO 44 (POWER_ENABLE) and GPIO 43 (POWER_HOLD)
  // Power-off sequence:
  //   1. Set POWER_HOLD (GPIO 43) LOW to release power hold
  //   2. Set POWER_ENABLE (GPIO 44) LOW to disable power output
  //   3. Call esp_deep_sleep() to enter deep sleep mode
  
  LOG_I("Initiating PMS150G power-off sequence");
  
  // Configure GPIO pins as output if not already done
  gpio_config_t gpio_cfg = {0};
  gpio_cfg.pin_bit_mask = (1ULL << GPIO_NUM_44) | (1ULL << GPIO_NUM_43);
  gpio_cfg.mode = GPIO_MODE_OUTPUT;
  gpio_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&gpio_cfg);
  
  // Execute power-off sequence
  gpio_set_level(GPIO_NUM_44, 0);  // POWER_ENABLE = LOW
  gpio_set_level(GPIO_NUM_43, 0);  // POWER_HOLD = LOW
  
  LOG_I("Power control signals set to OFF state");
  
  // Enter deep sleep (will not return from here)
  esp_deep_sleep_start();
  
  return false;  // Never reached
}

bool M5Paper3PowerManager::set_power_button_wakeup()
{
  LOG_I("M5Paper3PowerManager::set_power_button_wakeup()");
  
  if (!initialized) {
    LOG_E("Power manager not initialized");
    return false;
  }

  // Configure physical button GPIO (GPIO 0) for wakeup from deep sleep
  // Button is pull-up sensing with GND short = pressed (LOW = active)
  
  LOG_I("Configuring physical button (GPIO 0) for deep sleep wakeup");
  
  // Configure GPIO 0 as input with pull-up
  gpio_config_t gpio_cfg = {0};
  gpio_cfg.pin_bit_mask = 1ULL << GPIO_NUM_0;
  gpio_cfg.mode = GPIO_MODE_INPUT;
  gpio_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_cfg.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpio_cfg);
  
  // Enable external wake-up on GPIO 0, triggered on LOW (button press)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  
  LOG_I("Power button wakeup enabled: Press to wake device (GPIO 0, LOW active)");
  return true;
}

// ============ RTC Wakeup Methods ============

bool M5Paper3PowerManager::setup_rtc()
{
  LOG_I("M5Paper3PowerManager::setup_rtc()");
  
  if (rtc != nullptr) {
    LOG_W("RTC already initialized");
    return rtc_initialized;
  }

  rtc = new BM8563RTC();
  if (rtc == nullptr) {
    LOG_E("Failed to allocate RTC object");
    return false;
  }

  if (!rtc->setup()) {
    LOG_E("RTC setup failed - device not found or not responding");
    delete rtc;
    rtc = nullptr;
    rtc_initialized = false;
    return false;
  }

  LOG_I("RTC initialized successfully");
  rtc_initialized = true;
  return true;
}

bool M5Paper3PowerManager::set_rtc_wakeup(time_t wakeup_time)
{
  LOG_I("M5Paper3PowerManager::set_rtc_wakeup()");
  
  if (!rtc_initialized || rtc == nullptr) {
    LOG_E("RTC not initialized");
    return false;
  }

  if (!rtc->is_present()) {
    LOG_E("RTC not present on I2C bus");
    return false;
  }

  // Convert wakeup time to components
  struct tm time;
  if (gmtime_r(&wakeup_time, &time) == nullptr) {
    LOG_E("Failed to convert time_t");
    return false;
  }

  LOG_I("Setting RTC wakeup for: %04d-%02d-%02d %02d:%02d:%02d",
        time.tm_year + 1900, time.tm_mon + 1, time.tm_mday,
        time.tm_hour, time.tm_min, time.tm_sec);

  // Set alarm time (BM8563 alarms typically at minute resolution)
  rtc->set_alarm(
    static_cast<uint8_t>(time.tm_min),
    static_cast<uint8_t>(time.tm_hour),
    static_cast<uint8_t>(time.tm_mday),
    static_cast<uint8_t>(time.tm_wday)
  );

  rtc->enable_alarm();
  rtc->clear_alarm_flag();

  LOG_I("RTC wakeup alarm set and enabled");
  return true;
}

bool M5Paper3PowerManager::set_rtc_timer_wakeup(uint8_t seconds)
{
  LOG_I("M5Paper3PowerManager::set_rtc_timer_wakeup() - %d seconds", seconds);
  
  if (!rtc_initialized || rtc == nullptr) {
    LOG_E("RTC not initialized");
    return false;
  }

  if (!rtc->is_present()) {
    LOG_E("RTC not present on I2C bus");
    return false;
  }

  // Map seconds to RTC timer frequency and value
  // BM8563 timer frequencies:
  // - FREQ_4096_HZ: 122 microseconds per tick
  // - FREQ_64_HZ: 15.625 milliseconds per tick
  // - FREQ_1_HZ: 1 second per tick
  // - FREQ_1_60_HZ: 1/60 second per tick

  uint8_t timer_value = seconds;
  BM8563RTC::TimerFrequency freq = BM8563RTC::TimerFrequency::FREQ_1_HZ;

  if (seconds > 255) {
    LOG_W("Timer duration %d seconds exceeds maximum 255 seconds", seconds);
    timer_value = 255;
  }

  LOG_D("Setting RTC timer: freq=%d, value=%d", static_cast<uint8_t>(freq), timer_value);

  rtc->set_timer(freq, timer_value);
  rtc->enable_timer();
  rtc->clear_timer_flag();

  LOG_I("RTC timer wakeup set for %d seconds", timer_value);
  return true;
}

bool M5Paper3PowerManager::clear_rtc_alarm()
{
  LOG_D("M5Paper3PowerManager::clear_rtc_alarm()");
  
  if (!rtc_initialized || rtc == nullptr) {
    LOG_E("RTC not initialized");
    return false;
  }

  if (!rtc->is_present()) {
    LOG_E("RTC not present on I2C bus");
    return false;
  }

  rtc->clear_alarm_flag();
  rtc->disable_alarm();

  LOG_D("RTC alarm cleared and disabled");
  return true;
}

bool M5Paper3PowerManager::is_rtc_ready()
{
  return rtc_initialized && rtc != nullptr && rtc->is_present();
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
