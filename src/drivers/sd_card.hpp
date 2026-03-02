#pragma once

#include <cinttypes>

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#if !M5_PAPER_S3
  #if PCAL6416
    #include "pcal6416.hpp"
  #else
    #include "mcp23017.hpp"
  #endif
#endif

class SDCard
{
  public:
#if !M5_PAPER_S3
    SDCard(IOExpander & _io_expander) : io_expander(_io_expander) {}
#else
    SDCard() {}
#endif

    /**
     * @brief SD-Card Setup
     * 
     * This method initialize the ESP-IDF SD-Card capability. This will allow access
     * to the card through standard Posix IO functions or the C++ IOStream.
     * 
     * @return true Initialization Done
     * @return false Some issue
     */
    bool setup();
    void deepSleep();
 
  private:
    static constexpr char const * TAG = "SDCard";

#if !M5_PAPER_S3
    #if INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
      static constexpr IOExpander::Pin SD_POWER = IOExpander::Pin::IOPIN_13;
    #endif
#endif

    static constexpr gpio_num_t PIN_NUM_MISO = GPIO_NUM_12;
    static constexpr gpio_num_t PIN_NUM_MOSI = GPIO_NUM_13;
    static constexpr gpio_num_t PIN_NUM_CLK  = GPIO_NUM_14;
    static constexpr gpio_num_t PIN_NUM_CS   = GPIO_NUM_15;

#if !M5_PAPER_S3
    IOExpander & io_expander;
#endif
    enum class SDCardState : uint8_t { UNINITIALIZED, INITIALIZED, FAILED };
    SDCardState state{SDCardState::UNINITIALIZED};
    sdmmc_card_t *card{nullptr};
};
