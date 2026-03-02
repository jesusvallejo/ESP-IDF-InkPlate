/*
spi_bus_manager.hpp
SPI Bus Arbitration Manager

Manages shared SPI bus between display (IT8951E) and microSD card
Multiple devices can share a single SPI bus with chip select arbitration

This code is released under the GNU Lesser General Public License v3.0
*/

#pragma once

#include <cinttypes>
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @brief SPI Bus Manager for multi-device arbitration
 * 
 * Manages access to a shared SPI bus used by:
 * - IT8951E display controller
 * - microSD card reader
 * 
 * Uses mutex-based locking to prevent bus contention.
 */

class SPIBusManager
{
  public:
    enum class SPIDevice {
      DISPLAY_IT8951,
      SDCARD,
      INVALID
    };

    SPIBusManager();
    ~SPIBusManager();

    // Bus initialization
    bool init();
    bool is_initialized() { return initialized; }

    // Device handle acquisition
    spi_device_handle_t get_device_handle(SPIDevice device);

    // Bus locking for atomic operations
    struct SPITransaction {
      SPIDevice device;
      TickType_t timeout;
      bool valid;
    };

    SPITransaction acquire_bus(SPIDevice device, uint32_t timeout_ms);
    void release_bus(SPITransaction & trans);

  private:
    static constexpr char const * TAG = "SPIBusManager";

    bool initialized;
    SemaphoreHandle_t bus_mutex;
    
    spi_device_handle_t spi_display;
    spi_device_handle_t spi_sdcard;

    // SPI bus configuration
    static constexpr int SPI_BUS_ID = HSPI_HOST;  // Using HSPI (SPI2) for M5Paper3
    static constexpr int SPI_DISPLAY_PIN_CS = 12;
    static constexpr int SPI_SDCARD_PIN_CS = 14;

    // SPI frequency settings
    static constexpr int SPI_FREQ_DISPLAY = 10 * 1000 * 1000;  // 10MHz for display
    static constexpr int SPI_FREQ_SDCARD  = 20 * 1000 * 1000;  // 20MHz for SD card

    bool configure_display_device();
    bool configure_sdcard_device();
    bool reconfigure_cs_pins(SPIDevice current_device);
};
