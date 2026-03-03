#pragma once

#if M5_PAPER_S3

#include "non_copyable.hpp"
#include "wire.hpp"
#include <array>

/**
 * @brief GT911 Capacitive Touch Controller for M5 Paper S3
 * 
 * Provides touch input handling for the M5 Paper S3 device.
 * GT911 is an I2C-based capacitive touch controller.
 * 
 * Features:
 * - I2C address: 0x14 (0x28 with R/W bit)
 * - Resolution: 960×540 pixels (configured in m5_paper_s3.cpp)
 * - Interrupt pin: GPIO 48
 * - Dual touch point detection (up to 5 touch points capable)
 */
class TouchScreen : NonCopyable 
{
  public:

    static constexpr gpio_num_t INTERRUPT_PIN = GPIO_NUM_48;
    static constexpr uint16_t GT911_I2C_ADDR = 0x14;

    // M5 Paper S3 display resolution
    static constexpr uint16_t MAX_X = 960;
    static constexpr uint16_t MAX_Y = 540;

    TouchScreen() : ready(false), touch_pressed(false), last_x(0), last_y(0), app_isr_handler(nullptr) {}

    typedef void (* ISRHandlerPtr)(void * value);
    typedef std::array<uint16_t, 2> TouchPositions;
    
    /**
     * @brief Initialize GT911 touch controller
     * @param power_on Whether to enable touch on startup
     * @param isr_handler Optional interrupt service routine handler
     * @return true if initialization successful
     */
    bool setup(bool power_on, ISRHandlerPtr isr_handler = nullptr);

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
     * @param x_positions Array to store X coordinates (up to 2 points)
     * @param y_positions Array to store Y coordinates (up to 2 points)
     * @return Number of touch points detected (0-2)
     */
    uint8_t get_position(TouchPositions & x_positions, TouchPositions & y_positions);

    bool is_ready() { return ready; }

    void set_app_isr_handler(ISRHandlerPtr isr_handler) { 
      app_isr_handler = isr_handler;
    }

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

  private:
    bool ready;
    bool touch_pressed;
    uint16_t last_x = 0;
    uint16_t last_y = 0;
    ISRHandlerPtr app_isr_handler = nullptr;

    /**
     * @brief Read GT911 touch data from I2C
     * @return Number of touch points detected
     */
    uint8_t read_touch_data();
    
    /**
     * @brief ISR handler for touch interrupt
     */
    static void touch_interrupt_handler(void * arg);
};

// Global GT911 touch screen instance for M5 Paper S3
extern TouchScreen touch_screen;

#endif // M5_PAPER_S3
