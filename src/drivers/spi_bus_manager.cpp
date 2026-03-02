/*
spi_bus_manager.cpp
SPI Bus Arbitration Manager Implementation

This code is released under the GNU Lesser General Public License v3.0
*/

#include "spi_bus_manager.hpp"
#include "esp_log.h"

static constexpr char const * TAG = "SPIBusManager";

SPIBusManager::SPIBusManager()
  : initialized(false), bus_mutex(nullptr), spi_display(nullptr), spi_sdcard(nullptr)
{
  LOG_I("SPIBusManager constructor");
}

SPIBusManager::~SPIBusManager()
{
  LOG_I("SPIBusManager destructor");
  if (bus_mutex != nullptr) {
    vSemaphoreDelete(bus_mutex);
  }
}

bool SPIBusManager::init()
{
  LOG_I("SPIBusManager::init()");

  // Create mutex for bus access
  bus_mutex = xSemaphoreCreateMutex();
  if (bus_mutex == nullptr) {
    LOG_E("Failed to create bus mutex");
    return false;
  }

  // TODO: Initialize SPI host
  // TODO: Configure display device
  // TODO: Configure SD card device

  initialized = true;
  LOG_I("SPIBusManager initialized successfully");
  return true;
}

spi_device_handle_t SPIBusManager::get_device_handle(SPIDevice device)
{
  LOG_D("SPIBusManager::get_device_handle()");
  
  if (!initialized) {
    LOG_E("Manager not initialized");
    return nullptr;
  }

  switch (device) {
    case SPIDevice::DISPLAY_IT8951:
      return spi_display;
    case SPIDevice::SDCARD:
      return spi_sdcard;
    default:
      LOG_E("Invalid SPI device");
      return nullptr;
  }
}

SPIBusManager::SPITransaction SPIBusManager::acquire_bus(SPIDevice device, uint32_t timeout_ms)
{
  SPITransaction trans;
  trans.device = SPIDevice::INVALID;
  trans.valid = false;

  if (!initialized) {
    LOG_E("Manager not initialized");
    return trans;
  }

  TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

  // Try to acquire mutex
  if (xSemaphoreTake(bus_mutex, timeout_ticks) != pdTRUE) {
    LOG_W("Failed to acquire SPI bus - timeout");
    return trans;
  }

  // TODO: Configure CS pins for current device
  LOG_D("SPI bus acquired for device %d", static_cast<int>(device));

  trans.device = device;
  trans.timeout = timeout_ticks;
  trans.valid = true;

  return trans;
}

void SPIBusManager::release_bus(SPITransaction & trans)
{
  if (!trans.valid) {
    LOG_W("Attempting to release invalid transaction");
    return;
  }

  LOG_D("SPIBusManager::release_bus()");
  xSemaphoreGive(bus_mutex);
  trans.valid = false;
}

bool SPIBusManager::configure_display_device()
{
  LOG_I("SPIBusManager::configure_display_device()");
  // TODO: Configure SPI device for IT8951E
  return true;
}

bool SPIBusManager::configure_sdcard_device()
{
  LOG_I("SPIBusManager::configure_sdcard_device()");
  // TODO: Configure SPI device for SD card
  return true;
}

bool SPIBusManager::reconfigure_cs_pins(SPIDevice current_device)
{
  LOG_D("SPIBusManager::reconfigure_cs_pins()");
  // TODO: Reconfigure GPIO CS pins for current device
  return true;
}
