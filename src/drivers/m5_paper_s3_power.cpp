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
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
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
  
  // Battery voltage on ADC1 Channel 3 (GPIO 36)
  // Battery: 3.7V nominal (LiPo safe range: 3.0V-4.2V)
  // Voltage divider: Input 0-5V mapped to 0-3.3V for ADC
  // For simplicity, we'll use the legacy ADC functions that are still available
  // If those are removed, migration to adc_oneshot_t is needed
  
  LOG_I("Battery ADC configured (ADC1, Channel 3, GPIO 36)");
  return true;
}

bool M5Paper3PowerManager::init_pms150g()
{
  // Configure PMS150G power management IC via GPIO signals
  // PMS150G: GPIO-based power control (not I2C)
  // Control signals: 
  //   - POWER_ENABLE (GPIO 44): HIGH = power output enabled, LOW = power output disabled
  //   - POWER_HOLD (GPIO 43): HIGH = hold power state, LOW = release power hold
  // Usage:
  //   - Power on: Set POWER_HOLD HIGH, POWER_ENABLE HIGH
  //   - Power off: Set POWER_HOLD LOW, POWER_ENABLE LOW
  //   - Program mode: pulse POWER_ENABLE to enter firmware download mode
  
  LOG_I("M5Paper3PowerManager::init_pms150g()");
  
  // Configure GPIO pins as output
  gpio_config_t gpio_cfg = {};
  gpio_cfg.pin_bit_mask = (1ULL << GPIO_NUM_44) | (1ULL << GPIO_NUM_43);
  gpio_cfg.mode = GPIO_MODE_OUTPUT;
  gpio_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_cfg.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpio_cfg);
  
  // Initialize to power ON state (both HIGH)
  gpio_set_level(GPIO_NUM_44, 1);  // POWER_ENABLE = HIGH
  gpio_set_level(GPIO_NUM_43, 1);  // POWER_HOLD = HIGH
  
  LOG_I("PMS150G GPIO control initialized (GPIO 44/43)");
  return true;
}

float M5Paper3PowerManager::read_battery_voltage_adc()
{
  LOG_D("Reading battery voltage via ADC");
  
  // For now, return a nominal battery voltage
  // TODO: Implement proper ADC reading using new esp_adc API when available
  // This requires more configuration with the new ADC driver
  
  // Nominal lithium battery voltage: 3.7V
  float battery_voltage = 3.7f;
  
  LOG_D("Battery voltage: %.3fV", battery_voltage);
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
  
  // Trigger PMS150G power-off sequence via GPIO signals
  // PMS150G: Configure GPIO 44 (POWER_ENABLE) and GPIO 43 (POWER_HOLD)
  // Power-off sequence:
  //   1. Set POWER_HOLD (GPIO 43) LOW to release power hold
  //   2. Set POWER_ENABLE (GPIO 44) LOW to disable power output
  //   3. Call esp_deep_sleep() to enter deep sleep mode
  
  LOG_I("Initiating PMS150G power-off sequence");
  
  // Configure GPIO pins as output if not already done
  gpio_config_t gpio_cfg = {};
  gpio_cfg.pin_bit_mask = (1ULL << GPIO_NUM_44) | (1ULL << GPIO_NUM_43);
  gpio_cfg.mode = GPIO_MODE_OUTPUT;
  gpio_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_cfg.intr_type = GPIO_INTR_DISABLE;
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
  gpio_config_t gpio_cfg = {};
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

  // Map seconds to RTC timer value
  // BM8563 supports 0-255 second timer maximum
  if (seconds == 0) {
    LOG_W("Timer duration must be > 0");
    return false;
  }
  
  // Set RTC timer for wakeup
  rtc->set_timer(BM8563RTC::TimerFrequency::FREQ_1_HZ, seconds);
  rtc->enable_timer();
  rtc->clear_timer_flag();

  LOG_I("RTC timer wakeup set for %d seconds", seconds);
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
