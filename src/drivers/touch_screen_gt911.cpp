#if M5_PAPER_S3

#include "touch_screen_gt911.hpp"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char * TAG = "GT911";

// Global GT911 touch screen instance for M5 Paper S3
TouchScreen touch_screen;

// GT911 chip ID
static constexpr uint8_t GT911_CHIP_ID = 0x11;
static constexpr uint16_t GT911_CHIP_ID_REG = 0x8140;

/**
 * @brief Initialize GT911 touch controller via I2C
 */
bool TouchScreen::setup(bool power_on, ISRHandlerPtr isr_handler) 
{
  app_isr_handler = isr_handler;
  
  ESP_LOGI(TAG, "Initializing GT911 touch controller at I2C address 0x%02X", GT911_I2C_ADDR);

  // Use the GT911 device handle created by m5_paper_s3 driver
  extern i2c_master_dev_handle_t gt911_device_handle;
  i2c_dev = gt911_device_handle;
  
  if (!i2c_dev) {
    ESP_LOGE(TAG, "GT911 device handle not initialized by m5_paper_s3 driver");
    return false;
  }

  // Verify GT911 chip ID
  uint8_t chip_id = 0;
  if (!read_register(GT911_CHIP_ID_REG, &chip_id, 1)) {
    ESP_LOGE(TAG, "Failed to read GT911 chip ID");
    return false;
  }

  if (chip_id != GT911_CHIP_ID) {
    ESP_LOGW(TAG, "Unexpected GT911 chip ID: 0x%02X (expected 0x%02X)", chip_id, GT911_CHIP_ID);
    // Continue anyway - some GT911 clones may have different IDs
  } else {
    ESP_LOGI(TAG, "GT911 chip ID verified: 0x%02X", chip_id);
  }

  // Configure GT911 interrupt pin
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_NEGEDGE;      // Falling edge trigger
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = (1ULL << INTERRUPT_PIN);
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

  if (gpio_config(&io_conf) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure GT911 interrupt GPIO");
    return false;
  }

  // Install GPIO ISR handler
  if (gpio_install_isr_service(0) == ESP_OK) {
    if (gpio_isr_handler_add(INTERRUPT_PIN, touch_interrupt_handler, this) != ESP_OK) {
      ESP_LOGW(TAG, "Failed to add GPIO ISR handler");
    } else {
      ESP_LOGI(TAG, "GT911 GPIO interrupt handler installed");
    }
  }

  ready = true;
  touch_pressed = false;
  last_touch_count = 0;
  
  if (power_on) {
    set_power_state(true);
  }

  ESP_LOGI(TAG, "GT911 touch controller initialized successfully");
  return true;
}

/**
 * @brief Perform hardware reset of GT911
 */
bool TouchScreen::reset()
{
  ESP_LOGI(TAG, "Performing GT911 hardware reset");
  
  // Reset GT911 using INT pin
  // Pull INT low for at least 5ms, then release
  gpio_set_direction(INTERRUPT_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(INTERRUPT_PIN, 0);
  vTaskDelay(pdMS_TO_TICKS(10));
  gpio_set_level(INTERRUPT_PIN, 1);
  vTaskDelay(pdMS_TO_TICKS(10));
  
  // Reconfigure as input
  gpio_set_direction(INTERRUPT_PIN, GPIO_MODE_INPUT);
  gpio_set_pull_mode(INTERRUPT_PIN, GPIO_PULLUP_ONLY);
  
  vTaskDelay(pdMS_TO_TICKS(100));  // Wait for GT911 to stabilize
  
  ESP_LOGI(TAG, "GT911 hardware reset completed");
  return true;
}

void TouchScreen::set_power_state(bool on_state)
{
  if (on_state) {
    ESP_LOGD(TAG, "GT911 touch enabled");
    ready = true;
  } else {
    ESP_LOGD(TAG, "GT911 touch disabled");
    ready = false;
  }
}

bool TouchScreen::get_power_state()
{
  return ready;
}

void TouchScreen::shutdown(bool remove_handler) 
{
  if (remove_handler) {
    gpio_isr_handler_remove(INTERRUPT_PIN);
    ESP_LOGD(TAG, "GT911 GPIO ISR handler removed");
  }
  
  // Don't try to remove the device from I2C bus - it's managed by m5_paper_s3 driver
  i2c_dev = nullptr;
  
  ready = false;
  touch_pressed = false;
  ESP_LOGI(TAG, "GT911 touch controller shutdown");
}

bool TouchScreen::is_screen_touched()
{
  uint8_t touch_count = read_touch_data();
  return touch_count > 0;
}

uint8_t TouchScreen::get_position(TouchPositions & x_positions, TouchPositions & y_positions)
{
  uint8_t touch_count = read_touch_data();
  
  if (touch_count > 0) {
    for (int i = 0; i < touch_count && i < MAX_TOUCH_POINTS; i++) {
      // For backward compatibility, return first touch point
      if (i == 0) {
        x_positions[0] = last_x;
        y_positions[0] = last_y;
      }
    }
  }
  
  return touch_count;
}

/**
 * @brief Read register(s) from GT911 via I2C
 */
bool TouchScreen::read_register(uint16_t reg, uint8_t * data, uint8_t len)
{
  if (!i2c_dev) {
    ESP_LOGE(TAG, "I2C device not initialized");
    return false;
  }

  // GT911 uses 16-bit register addresses (big-endian)
  uint8_t reg_addr[2] = {
    (uint8_t)((reg >> 8) & 0xFF),
    (uint8_t)(reg & 0xFF)
  };

  esp_err_t err = i2c_master_transmit_receive(i2c_dev, reg_addr, sizeof(reg_addr), 
                                              data, len, 50);
  
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C read from register 0x%04X failed: %s", reg, esp_err_to_name(err));
    return false;
  }

  return true;
}

/**
 * @brief Write register(s) to GT911 via I2C
 */
bool TouchScreen::write_register(uint16_t reg, const uint8_t * data, uint8_t len)
{
  if (!i2c_dev) {
    ESP_LOGE(TAG, "I2C device not initialized");
    return false;
  }

  // Prepare buffer: 2-byte address + data
  uint8_t buffer[len + 2];
  buffer[0] = (uint8_t)((reg >> 8) & 0xFF);
  buffer[1] = (uint8_t)(reg & 0xFF);
  std::memcpy(&buffer[2], data, len);

  esp_err_t err = i2c_master_transmit(i2c_dev, buffer, len + 2, 50);
  
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C write to register 0x%04X failed: %s", reg, esp_err_to_name(err));
    return false;
  }

  return true;
}

/**
 * @brief Calculate checksum for GT911 configuration
 */
uint8_t TouchScreen::calculate_checksum(const uint8_t * config, uint8_t size)
{
  uint16_t sum = 0;
  for (int i = 0; i < size; i++) {
    sum += config[i];
  }
  return (~(sum & 0xFF)) + 1;
}

/**
 * @brief Read GT911 status and return touch count
 */
uint8_t TouchScreen::read_touch_data()
{
  if (!ready || !i2c_dev) {
    return 0;
  }

  uint8_t status = 0;
  if (!read_register(GT911Registers::STATUS, &status, 1)) {
    return 0;
  }

  // Check if data is ready
  if (!(status & 0x80)) {
    touch_pressed = false;
    return 0;
  }

  // Extract touch count from status register (bits 3-0)
  uint8_t touch_count = status & 0x0F;

  if (touch_count == 0) {
    touch_pressed = false;
    last_touch_count = 0;
    return 0;
  }

  if (touch_count > MAX_TOUCH_POINTS) {
    touch_count = MAX_TOUCH_POINTS;
  }

  // Read touch data
  uint8_t touch_data[TOUCH_DATA_SIZE * touch_count];
  if (!read_register(GT911Registers::TOUCH_DATA_BASE, touch_data, TOUCH_DATA_SIZE * touch_count)) {
    return 0;
  }

  touch_pressed = true;
  last_touch_count = touch_count;

  // Parse first touch point for backward compatibility
  if (touch_count > 0) {
    uint16_t x = ((touch_data[0] & 0x0F) << 8) | touch_data[1];
    uint16_t y = ((touch_data[2] & 0x0F) << 8) | touch_data[3];
    
    last_x = x;
    last_y = y;
  }

  // Clear interrupt by writing 0 to status register
  clear_interrupt();

  return touch_count;
}

/**
 * @brief Parse raw touch data into TouchData structure
 */
TouchScreen::TouchData TouchScreen::parse_touch_data(uint8_t raw_status)
{
  TouchData result = {};
  result.num_touch_points = 0;
  result.gesture_id = 0;
  result.has_checksum_error = false;

  if (!(raw_status & 0x80)) {
    return result;  // No data ready
  }

  uint8_t touch_count = raw_status & 0x0F;
  if (touch_count > MAX_TOUCH_POINTS) {
    touch_count = MAX_TOUCH_POINTS;
  }

  result.num_touch_points = touch_count;

  if (touch_count == 0) {
    return result;
  }

  // Read gesture ID
  uint8_t gesture_data = 0;
  if (read_register(0x814F, &gesture_data, 1)) {
    result.gesture_id = gesture_data;
  }

  // Read touch points
  uint8_t touch_data[TOUCH_DATA_SIZE * MAX_TOUCH_POINTS] = {0};
  if (read_register(GT911Registers::TOUCH_DATA_BASE, touch_data, TOUCH_DATA_SIZE * touch_count)) {
    for (int i = 0; i < touch_count; i++) {
      uint8_t offset = i * TOUCH_DATA_SIZE;
      
      TouchPoint & point = result.touch_points[i];
      
      // Extract coordinates (12-bit values, little-endian)
      point.x = ((touch_data[offset + 0] & 0x0F) << 8) | touch_data[offset + 1];
      point.y = ((touch_data[offset + 2] & 0x0F) << 8) | touch_data[offset + 3];
      
      // Extract size and ID
      point.size = touch_data[offset + 4];
      point.pressure = touch_data[offset + 5];
      point.id = touch_data[offset + 6];
      
      point.valid = (point.x > 0 || point.y > 0);
    }
  }

  return result;
}

/**
 * @brief Get complete touch data for all points
 */
TouchScreen::TouchData TouchScreen::get_touch_data()
{
  if (!ready || !i2c_dev) {
    TouchData empty = {};
    return empty;
  }

  uint8_t status = 0;
  if (!read_register(GT911Registers::STATUS, &status, 1)) {
    TouchData empty = {};
    return empty;
  }

  return parse_touch_data(status);
}

/**
 * @brief Clear GT911 interrupt status
 */
bool TouchScreen::clear_interrupt()
{
  uint8_t clear_byte = 0x00;
  return write_register(GT911Registers::STATUS, &clear_byte, 1);
}

/**
 * @brief Set touch sensitivity
 */
void TouchScreen::set_touch_sensitivity(uint8_t sensitivity)
{
  if (!i2c_dev || sensitivity == 0) {
    return;
  }

  // Write sensitivity to GT911 configuration register (register 0x8062)
  uint8_t data = sensitivity;
  write_register(0x8062, &data, 1);
  
  ESP_LOGD(TAG, "GT911 sensitivity set to 0x%02X", sensitivity);
}

/**
 * @brief Set noise reduction
 */
void TouchScreen::set_noise_reduction(uint8_t reduction)
{
  if (!i2c_dev || reduction == 0) {
    return;
  }

  // Write noise reduction to GT911 configuration register (register 0x8063)
  uint8_t data = reduction;
  write_register(0x8063, &data, 1);
  
  ESP_LOGD(TAG, "GT911 noise reduction set to 0x%02X", reduction);
}

/**
 * @brief Set debounce time
 */
void TouchScreen::set_debounce_time(uint8_t time_ms)
{
  if (!i2c_dev) {
    return;
  }

  // Write debounce time to GT911 configuration register (register 0x8064)
  uint8_t data = time_ms / 10;  // GT911 uses 10ms units
  write_register(0x8064, &data, 1);
  
  ESP_LOGD(TAG, "GT911 debounce time set to %d ms", time_ms);
}

/**
 * @brief GT911 ISR handler
 */
void TouchScreen::touch_interrupt_handler(void * arg) 
{
  TouchScreen * this_ptr = static_cast<TouchScreen *>(arg);
  
  if (this_ptr && this_ptr->app_isr_handler) {
    this_ptr->app_isr_handler(arg);
  }
}

#endif // M5_PAPER_S3
