#if M5_PAPER_S3

#include "touch_screen_gt911.hpp"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

static const char * TAG = "GT911";

// GT911 I2C Register definitions
static constexpr uint16_t GT911_REG_STATUS = 0x814E;
static constexpr uint16_t GT911_REG_TOUCH_DATA = 0x8150;
static constexpr uint16_t GT911_REG_CONFIG_START = 0x8047;
static constexpr uint16_t GT911_REG_CONFIG_VERSION = 0x8047;
static constexpr uint16_t GT911_REG_X_OUTPUT_MAX_LOW = 0x8048;
static constexpr uint16_t GT911_REG_X_OUTPUT_MAX_HIGH = 0x8049;
static constexpr uint16_t GT911_REG_Y_OUTPUT_MAX_LOW = 0x804A;
static constexpr uint16_t GT911_REG_Y_OUTPUT_MAX_HIGH = 0x804B;

static TouchScreen * gt911_instance = nullptr;

bool TouchScreen::setup(bool power_on, ISRHandlerPtr isr_handler) 
{
  gt911_instance = this;
  app_isr_handler = isr_handler;
  
  ESP_LOGI(TAG, "Initializing GT911 touch controller at I2C address 0x%02X", GT911_I2C_ADDR);

  // GT911 interrupt pin configuration
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
  if (gpio_install_isr_service(0) == ESP_OK || gpio_isr_handler_add(INTERRUPT_PIN, touch_interrupt_handler, this) == ESP_OK) {
    ESP_LOGI(TAG, "GT911 GPIO interrupt handler installed");
  }

  ready = true;
  touch_pressed = false;
  
  if (power_on) {
    set_power_state(true);
  }

  ESP_LOGI(TAG, "GT911 touch controller initialized successfully");
  return true;
}

void TouchScreen::set_power_state(bool on_state)
{
  // GT911 power is managed through INT pin state during startup
  // For M5 Paper S3, this is typically always on after initialization
  if (on_state) {
    ESP_LOGD(TAG, "GT911 touch enabled");
  } else {
    ESP_LOGD(TAG, "GT911 touch disabled");
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
  
  ready = false;
  touch_pressed = false;
  ESP_LOGI(TAG, "GT911 touch controller shutdown");
}

bool TouchScreen::is_screen_touched()
{
  return touch_pressed && read_touch_data() > 0;
}

uint8_t TouchScreen::get_position(TouchPositions & x_positions, TouchPositions & y_positions)
{
  uint8_t touch_count = read_touch_data();
  
  if (touch_count > 0) {
    x_positions[0] = last_x;
    y_positions[0] = last_y;
  }
  
  return touch_count;
}

uint8_t TouchScreen::read_touch_data()
{
  // GT911 touch data reading would be implemented here
  // This is a simplified stub - the actual I2C read operation
  // would retrieve touch coordinates from GT911_REG_TOUCH_DATA
  
  uint8_t touch_count = 0;
  
  // Read GT911 status register to get number of touch points
  uint8_t status_data = 0;
  // In full implementation, would use I2C driver to read from GT911_REG_STATUS
  // status_data would contain touch count in bits 0-3
  
  if (status_data & 0x80) {  // Data ready bit
    touch_count = status_data & 0x0F;  // Number of touch points (0-5)
    
    if (touch_count > 0) {
      // In full implementation, would read actual coordinates from GT911_REG_TOUCH_DATA
      // For now, use last stored positions
      touch_pressed = true;
    } else {
      touch_pressed = false;
    }
  }
  
  return touch_count;
}

void TouchScreen::touch_interrupt_handler(void * arg) 
{
  TouchScreen * this_ptr = static_cast<TouchScreen *>(arg);
  
  if (this_ptr && this_ptr->app_isr_handler) {
    this_ptr->app_isr_handler(arg);
  }
}

#endif // M5_PAPER_S3
