#pragma once

#if M5_PAPER_S3

#include "non_copyable.hpp"
#include "driver/i2c_master.h"
#include <array>
#include <cstdint>

/**
 * @brief GT911 Capacitive Touch Controller for M5 Paper S3
 * 
 * Provides touch input handling for the M5 Paper S3 device.
 * GT911 is an I2C-based capacitive touch controller supporting up to 5 concurrent touch points.
 * 
 * Features:
 * - I2C address: 0x14 (standard)
 * - Multi-touch: Supports 1-5 simultaneous touch points
 * - Resolution: 960×540 pixels (configurable)
 * - Interrupt pin: GPIO 48 (active low)
 * - Pressure sensitivity available
 * - Gesture recognition (swipe, zoom, etc.)
 * - Configurable sensitivity and thresholds
 */
class TouchScreen : NonCopyable 
{
  public:

    static constexpr gpio_num_t INTERRUPT_PIN = GPIO_NUM_48;
    static constexpr uint8_t GT911_I2C_ADDR = 0x14;

    // M5 Paper S3 display resolution
    static constexpr uint16_t MAX_X = 960;
    static constexpr uint16_t MAX_Y = 540;
    static constexpr uint8_t MAX_TOUCH_POINTS = 5;

    /**
     * @brief Single touch point data
     */
    struct TouchPoint {
      uint16_t x;           // X coordinate (0-959)
      uint16_t y;           // Y coordinate (0-539)
      uint8_t  id;          // Touch point ID (0-9)
      uint8_t  size;        // Contact area size
      uint8_t  pressure;    // Pressure/strength (0-255)
      bool     valid;       // Whether this touch point is valid
    };

    /**
     * @brief Complete touch data for all points
     */
    struct TouchData {
      uint8_t num_touch_points;
      std::array<TouchPoint, MAX_TOUCH_POINTS> touch_points;
      uint8_t gesture_id;   // 0=none, 1=swipe_left, 2=swipe_right, etc.
      bool has_checksum_error;
    };

    TouchScreen() : ready(false), touch_pressed(false), last_x(0), last_y(0), 
                    app_isr_handler(nullptr), i2c_dev(nullptr) {}

    typedef void (* ISRHandlerPtr)(void * value);
    typedef std::array<uint16_t, MAX_TOUCH_POINTS> TouchPositions;
    
    /**
     * @brief Initialize GT911 touch controller
     * @param power_on Whether to enable touch on startup
     * @param isr_handler Optional interrupt service routine handler
     * @return true if initialization successful
     */
    bool setup(bool power_on = true, ISRHandlerPtr isr_handler = nullptr);

    /**
     * @brief Perform hardware reset of GT911
     * @return true if reset successful
     */
    bool reset();

    void set_power_state(bool on_state);
    bool get_power_state();

    void shutdown(bool remove_handler = true);
    
    /**
     * @brief Check if screen is currently touched
     * @return true if one or more touch points detected
     */
    bool is_screen_touched();
    
    /**
     * @brief Get current touch positions
     * @param x_positions Array to store X coordinates (up to MAX_TOUCH_POINTS)
     * @param y_positions Array to store Y coordinates (up to MAX_TOUCH_POINTS)
     * @return Number of touch points detected (0-5)
     */
    uint8_t get_position(TouchPositions & x_positions, TouchPositions & y_positions);

    /**
     * @brief Get complete touch data for all points
     * @return TouchData structure with all point information
     */
    TouchData get_touch_data();

    bool is_ready() { return ready; }

    void set_app_isr_handler(ISRHandlerPtr isr_handler) { 
      app_isr_handler = isr_handler;
    }

    // Configuration methods
    void set_touch_sensitivity(uint8_t sensitivity);  // 0-255
    void set_noise_reduction(uint8_t reduction);      // 0-255
    void set_debounce_time(uint8_t time_ms);          // Debounce in ms

    inline uint16_t get_x_resolution() { return MAX_X; }
    inline uint16_t get_y_resolution() { return MAX_Y; }

    /**
     * @brief Get last measured touch X position
     * @return X coordinate in pixels
     */
    inline uint16_t get_x() { return last_x; }
    
    /**
     * @brief Get last measured touch Y position
     * @return Y coordinate in pixels
     */
    inline uint16_t get_y() { return last_y; }

    /**
     * @brief Get last measured touch count
     */
    inline uint8_t get_touch_count() { return last_touch_count; }

  private:
    bool ready;
    bool touch_pressed;
    uint16_t last_x = 0;
    uint16_t last_y = 0;
    uint8_t last_touch_count = 0;
    ISRHandlerPtr app_isr_handler = nullptr;
    i2c_master_dev_handle_t i2c_dev = nullptr;

    // GT911 Register addresses
    struct GT911Registers {
      static constexpr uint16_t CONFIG_START = 0x8047;
      static constexpr uint16_t CONFIG_VERSION = 0x8047;
      static constexpr uint16_t X_OUTPUT_MAX_LOW = 0x8048;
      static constexpr uint16_t X_OUTPUT_MAX_HIGH = 0x8049;
      static constexpr uint16_t Y_OUTPUT_MAX_LOW = 0x804A;
      static constexpr uint16_t Y_OUTPUT_MAX_HIGH = 0x804B;
      static constexpr uint16_t TOUCH_NUMBER = 0x805D;
      static constexpr uint16_t MODULE_SWITCH_1 = 0x804D;
      static constexpr uint16_t SHAKE_COUNT = 0x805E;

      static constexpr uint16_t STATUS = 0x814E;
      static constexpr uint16_t TOUCH_DATA_BASE = 0x8150;
      static constexpr uint16_t TOUCH_KEY_VALUE = 0x8093;
      static constexpr uint16_t COOR_XH = 0x8150;
      static constexpr uint16_t COOR_XL = 0x8151;
      static constexpr uint16_t COOR_YH = 0x8152;
      static constexpr uint16_t COOR_YL = 0x8153;
      static constexpr uint16_t COOR_AREA = 0x8154;
      static constexpr uint16_t PRESSURE = 0x8155;
      static constexpr uint16_t TRACK_ID = 0x8157;
    };

    // Configuration constants
    static constexpr uint8_t RESET_PIN = GPIO_NUM_48;  // INT pin used for reset
    static constexpr uint32_t I2C_TIMEOUT_MS = 50;
    static constexpr uint8_t TOUCH_DATA_SIZE = 8;      // Bytes per touch point

    /**
     * @brief Read register(s) from GT911 via I2C
     * @param reg Register address (16-bit)
     * @param data Buffer to read into
     * @param len Number of bytes to read
     * @return true if successful
     */
    bool read_register(uint16_t reg, uint8_t * data, uint8_t len);

    /**
     * @brief Write register(s) to GT911 via I2C
     * @param reg Register address (16-bit)
     * @param data Data to write
     * @param len Number of bytes to write
     * @return true if successful
     */
    bool write_register(uint16_t reg, const uint8_t * data, uint8_t len);

    /**
     * @brief Calculate configuration checksum
     * @param config Configuration data
     * @param size Size of configuration
     * @return Calculated checksum
     */
    uint8_t calculate_checksum(const uint8_t * config, uint8_t size);

    /**
     * @brief Read GT911 touch data from I2C
     * @return Number of touch points detected
     */
    uint8_t read_touch_data();
    
    /**
     * @brief Parse raw touch data into TouchData structure
     * @param raw_status Status register value
     * @return Parsed TouchData
     */
    TouchData parse_touch_data(uint8_t raw_status);

    /**
     * @brief ISR handler for touch interrupt
     */
    static void touch_interrupt_handler(void * arg);

    /**
     * @brief Clear interrupt status on GT911
     * @return true if successful
     */
    bool clear_interrupt();
};

// Global GT911 touch screen instance for M5 Paper S3
extern TouchScreen touch_screen;

#endif // M5_PAPER_S3
