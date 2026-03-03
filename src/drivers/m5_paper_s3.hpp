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
#include "m5_paper_s3_pins.hpp"

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

    static const uint16_t WIDTH  = 960;   // In pixels
    static const uint16_t HEIGHT = 540;   // In pixels
    static const uint8_t  GRAYSCALE_BITS = 4;  // 16 levels (4-bit)
    
    static const uint32_t BITMAP_SIZE_4BIT = (WIDTH * HEIGHT) >> 1;  // In bytes
    static const uint16_t LINE_SIZE_4BIT   = WIDTH >> 1;             // In bytes

    inline int16_t  get_width()  { return WIDTH;  }
    inline int16_t  get_height() { return HEIGHT; }

    virtual inline FrameBuffer2Bit * new_frame_buffer_2bit() { return nullptr; }
    // For compatibility with base class
    virtual inline FrameBuffer1Bit * new_frame_buffer_1bit() { return nullptr; }
    virtual inline FrameBuffer3Bit * new_frame_buffer_3bit() { return nullptr; }
    
    // M5Paper3 uses 4-bit (16-level) grayscale
    // Note: FrameBuffer4Bit class would need to be created in base framebuffer hierarchy
    // For now, conversion happens directly from 1-bit/3-bit to internal 4-bit format
    // TODO: Create FrameBuffer4Bit class if multi-format support needed
    // virtual inline FrameBuffer4Bit * new_frame_buffer_4bit() { 
    //   return new FrameBuffer4BitX(WIDTH, HEIGHT); 
    // }

    bool setup();

    void update(FrameBuffer1Bit & frame_buffer);
    void update(FrameBuffer3Bit & frame_buffer);

    void partial_update(FrameBuffer1Bit & frame_buffer, bool force = false);
    
  private:
    static constexpr char const * TAG = "M5Paper3";

    // Frame buffer implementation (4-bit grayscale)
    // Note: FrameBuffer4Bit not defined in base class hierarchy
    // Conversion from 1-bit/3-bit to 4-bit happens directly in update() methods
    // TODO: Implement FrameBuffer4Bit in frame_buffer.hpp if needed
    // class FrameBuffer4BitX : public FrameBuffer4Bit {
    //   private:
    //     uint8_t data[BITMAP_SIZE_4BIT];
    //   public:
    //     FrameBuffer4BitX() : FrameBuffer4Bit(WIDTH, HEIGHT, BITMAP_SIZE_4BIT) {}
    //     uint8_t * get_data() { return data; }
    // };

    // Driver initialization
    bool init_spi();
    bool init_epd();
    bool init_touch();
    bool load_gt911_firmware();  // Load GT911 touch controller firmware and calibration
    bool init_power();

    // Display operations
    void wait_for_display_ready();
    void load_image_to_display(const uint8_t * buffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void refresh_display(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t mode);
};

#endif
