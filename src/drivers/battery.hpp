#pragma once

#include "non_copyable.hpp"
#include "esp_adc/adc_oneshot.h"

#if !M5_PAPER_S3
  #if PCAL6416
    #include "pcal6416.hpp"
  #else
    #include "mcp23017.hpp"
  #endif
#endif

class Battery : NonCopyable
{
  public:
#if !M5_PAPER_S3
    Battery(IOExpander & _io_expander) : io_expander(_io_expander) {}
#else
    Battery() {}
#endif
    bool setup();

    double read_level();

  private:
    static constexpr char const * TAG = "Battery";
#if !M5_PAPER_S3
    IOExpander & io_expander;

    const IOExpander::Pin BATTERY_SWITCH = IOExpander::Pin::IOPIN_9;
#endif

    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t adc_unit_config;
    adc_oneshot_chan_cfg_t adc_channel_config;

};
