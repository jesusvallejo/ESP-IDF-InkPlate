/*
it8951_controller.hpp
IT8951E Display Controller

Driver for IT8951E e-ink display controller
Communication: SPI

This code is released under the GNU Lesser General Public License v3.0
*/

#pragma once

#include <cinttypes>
#include <cstring>
#include "driver/spi_master.h"
#include "esp_log.h"

/**
 * @brief IT8951E e-ink display controller driver
 * 
 * This class manages low-level SPI communication with the IT8951E
 * display controller. It provides interfaces for:
 * - Device initialization and querying
 * - Register read/write operations
 * - Image loading
 * - Display refresh/update
 * - Power management
 */

class IT8951Controller
{
  public:
    IT8951Controller();
    ~IT8951Controller();

    // Initialization
    bool init(spi_device_handle_t spi_handle);
    bool is_initialized() { return initialized; }

    // Device information
    struct DeviceInfo {
      uint16_t width;
      uint16_t height;
      uint32_t firmware_version;
      uint16_t lut_version;
    };
    
    bool get_device_info(DeviceInfo & info);

    // Register operations (IT8951 protocol)
    bool write_register(uint16_t address, uint16_t data);
    bool read_register(uint16_t address, uint16_t & data);

    // Image and display operations
    bool load_image_area(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t * buffer);
    bool display_area(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t display_mode);

    // Display modes
    static constexpr uint8_t DISPLAY_MODE_INIT = 0;    // Initialize
    static constexpr uint8_t DISPLAY_MODE_DU   = 1;    // Direct Update
    static constexpr uint8_t DISPLAY_MODE_GC   = 2;    // Gray Clear
    static constexpr uint8_t DISPLAY_MODE_A2   = 4;    // A2 mode

    // Power management
    bool power_on();
    bool power_off();
    bool wait_for_ready();

  private:
    static constexpr char const * TAG = "IT8951";

    spi_device_handle_t spi_handle;
    bool initialized;

    // Low-level SPI operations
    bool write_command(uint16_t cmd);
    bool send_data(const uint8_t * data, uint32_t length);
    bool receive_data(uint8_t * data, uint32_t length);
    bool wait_for_display_engine();

    // IT8951 protocol constants
    static constexpr uint16_t CMD_WRITE_REG     = 0x0001;
    static constexpr uint16_t CMD_READ_REG      = 0x0002;
    static constexpr uint16_t CMD_LOAD_IMAGE    = 0x0020;
    static constexpr uint16_t CMD_DISPLAY_AREA  = 0x0034;
    static constexpr uint16_t CMD_STANDBY       = 0x0040;
    static constexpr uint16_t CMD_SLEEP         = 0x0041;
    static constexpr uint16_t CMD_GET_DEVINFO   = 0x0302;
    static constexpr uint16_t CMD_WAIT_DSPE_TRG = 0x0045;

    // Register addresses
    static constexpr uint16_t REG_SYS_RUN    = 0x0000;
    static constexpr uint16_t REG_STANDBY    = 0x0001;
    static constexpr uint16_t REG_SLEEP      = 0x0002;
};
