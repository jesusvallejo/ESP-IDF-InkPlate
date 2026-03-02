/*
m5_paper_s3.hpp
M5 Paper S3 ESP-IDF

M5Stack Paper S3 e-ink display driver

This code is released under the GNU Lesser General Public License v3.0: https://www.gnu.org/licenses/lgpl-3.0.en.html
*/

#if M5_PAPER_S3

#pragma once

#include <cinttypes>
#include <cstring>

#include "non_copyable.hpp"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "eink.hpp"
#include "frame_buffer.hpp"

/**
 * @brief Low level e-Ink display for M5 Paper S3
 * 
 * This class implements the low level methods required to control
 * and access the e-ink display of the M5 Paper S3 device via IT8951E
 * controller over SPI.
 * 
 * Features:
 * - 600x960 pixel e-ink display
 * - 4-grayscale levels via IT8951E
 * - SPI-based communication
 * - Shared SPI bus with microSD card
 * 
 * This is a singleton. It cannot be instantiated elsewhere.
 */

class M5Paper3 : public EInk, NonCopyable
{
  public:
    M5Paper3();

    static const uint16_t WIDTH  = 600;   // In pixels
    static const uint16_t HEIGHT = 960;   // In pixels
    static const uint8_t  GRAYSCALE_BITS = 2;  // 4 levels (2-bit)
    
    static const uint32_t BITMAP_SIZE_2BIT = (WIDTH * HEIGHT) >> 2;  // In bytes
    static const uint16_t LINE_SIZE_2BIT   = WIDTH >> 2;             // In bytes

    inline int16_t  get_width()  { return WIDTH;  }
    inline int16_t  get_height() { return HEIGHT; }

    virtual inline FrameBuffer2Bit * new_frame_buffer_2bit() { return new FrameBuffer2BitX; }
    // For compatibility with base class
    virtual inline FrameBuffer1Bit * new_frame_buffer_1bit() { return nullptr; }
    virtual inline FrameBuffer3Bit * new_frame_buffer_3bit() { return nullptr; }

    bool setup();

    void update(FrameBuffer1Bit & frame_buffer);
    void update(FrameBuffer3Bit & frame_buffer);

    void partial_update(FrameBuffer1Bit & frame_buffer, bool force = false);
    
  private:
    static constexpr char const * TAG = "M5Paper3";

    // Frame buffer implementation
    class FrameBuffer2BitX : public FrameBuffer2Bit {
      private:
        uint8_t data[BITMAP_SIZE_2BIT];
      public:
        FrameBuffer2BitX() : FrameBuffer2Bit(WIDTH, HEIGHT, BITMAP_SIZE_2BIT) {}
       
        uint8_t * get_data() { return data; }
    };

    // Driver initialization
    bool init_spi();
    bool init_it8951();
    bool init_touch();
    bool init_power();

    // Display operations
    void wait_for_display_ready();
    void load_image_to_display(const uint8_t * buffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void refresh_display(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t mode);
};

#endif
