#if INKPLATE_6 || INKPLATE_6V2 || INKPLATE_6FLICK

#define __I2S_COMMS__ 1

#include "i2s_comms.hpp"

#define SPH 0x02 // GPIO33
#define SPH_SET                                                                                    \
  {                                                                                                \
    GPIO.out1_w1ts.val = SPH;                                                                      \
  }
#define SPH_CLEAR                                                                                  \
  {                                                                                                \
    GPIO.out1_w1tc.val = SPH;                                                                      \
  }

#define CKV 0x01
#define CKV_SET                                                                                    \
  {                                                                                                \
    GPIO.out1_w1ts.val = CKV;                                                                      \
  }
#define CKV_CLEAR                                                                                  \
  {                                                                                                \
    GPIO.out1_w1tc.val = CKV;                                                                      \
  }

/**
 **************************************************
 * @file        Esp.cpp
 * @brief       File for ESP, currently empty
 *
 *              https://github.com/e-radionicacom/Inkplate-Arduino-library
 *              For support, please reach over forums: forum.e-radionica.com/en
 *              For more info about the product, please check: www.inkplate.io
 *
 *              This code is released under the GNU Lesser General Public
 *License v3.0: https://www.gnu.org/licenses/lgpl-3.0.en.html Please review the
 *LICENSE file included with this example. If you have any questions about
 *licensing, please contact techsupport@e-radionica.com Distributed as-is; no
 *warranty is given.
 *
 * @authors     Soldered
 ***************************************************/

/**
 * @brief       Function Intializes I2S driver of the ESP32
 *
 * @param       i2s_dev_t *_i2sDev
 *              Pointer of the selected I2S driver
 *
 * @note        Function must be declared static to fit into Instruction RAM of the ESP32.
 */
void my_I2SInit(i2s_dev_t *_i2sDev, uint8_t _clockDivider) {
  // Enable I2S peripheral and reset it.
  periph_module_enable(PERIPH_I2S0_MODULE);
  periph_module_reset(PERIPH_I2S0_MODULE);

  // Reset the FIFO Buffer in I2S module.
  _i2sDev->conf.rx_fifo_reset = 1;
  _i2sDev->conf.rx_fifo_reset = 0;
  _i2sDev->conf.tx_fifo_reset = 1;
  _i2sDev->conf.tx_fifo_reset = 0;

  // Reset I2S DMA controller.
  _i2sDev->lc_conf.in_rst  = 1;
  _i2sDev->lc_conf.in_rst  = 0;
  _i2sDev->lc_conf.out_rst = 1;
  _i2sDev->lc_conf.out_rst = 0;

  // Reset I2S TX and RX module.
  _i2sDev->conf.rx_reset = 1;
  _i2sDev->conf.tx_reset = 1;
  _i2sDev->conf.rx_reset = 0;
  _i2sDev->conf.tx_reset = 0;

  // Set LCD mode on I2S, setup delays on SD and WR lines.
  _i2sDev->conf2.val            = 0;
  _i2sDev->conf2.lcd_en         = 1;
  _i2sDev->conf2.lcd_tx_wrx2_en = 1;
  _i2sDev->conf2.lcd_tx_sdx2_en = 0;

  _i2sDev->sample_rate_conf.val            = 0;
  _i2sDev->sample_rate_conf.rx_bits_mod    = 8;
  _i2sDev->sample_rate_conf.tx_bits_mod    = 8;
  _i2sDev->sample_rate_conf.rx_bck_div_num = 2;
  _i2sDev->sample_rate_conf.tx_bck_div_num = 2;

  // Do not use APLL, divide by 5 by default, BCK should be ~16MHz.
  _i2sDev->clkm_conf.val          = 0;
  _i2sDev->clkm_conf.clka_en      = 0;
  _i2sDev->clkm_conf.clkm_div_b   = 0;
  _i2sDev->clkm_conf.clkm_div_a   = 1;
  _i2sDev->clkm_conf.clkm_div_num = _clockDivider;

  // FIFO buffer setup. Byte packing for FIFO: 0A0B_0B0C = 0, 0A0B_0C0D = 1, 0A00_0B00 = 3. Use dual
  // mono single data
  _i2sDev->fifo_conf.val                  = 0;
  _i2sDev->fifo_conf.rx_fifo_mod_force_en = 1;
  _i2sDev->fifo_conf.tx_fifo_mod_force_en = 1;
  _i2sDev->fifo_conf.tx_fifo_mod =
      1; // byte packing 0A0B_0B0C = 0, 0A0B_0C0D = 1, 0A00_0B00 = 3. Use dual mono single data
  _i2sDev->fifo_conf.rx_data_num = 1;
  _i2sDev->fifo_conf.tx_data_num = 1;
  _i2sDev->fifo_conf.dscr_en     = 1;

  // Send BCK only when needed (needs to be powered on in einkOn() function and disabled in
  // einkOff()).
  _i2sDev->conf1.val           = 0;
  _i2sDev->conf1.tx_stop_en    = 0;
  _i2sDev->conf1.tx_pcm_bypass = 1;

  _i2sDev->conf_chan.val         = 0;
  _i2sDev->conf_chan.tx_chan_mod = 1;
  _i2sDev->conf_chan.rx_chan_mod = 1;

  _i2sDev->conf.tx_right_first = 0; //!!invert_clk; // should be false / 0
  _i2sDev->conf.rx_right_first = 0; //!!invert_clk;

  _i2sDev->timing.val = 0;
}

/**
 * @brief       Function sedns data with I2S DMA driver.
 *
 * @param       i2s_dev_t *_i2sDev
 *              Pointer of the selected I2S driver
 *
 *              lldesc_s *_dmaDecs
 *              Pointer to the DMA descriptor.
 *
 * @note        Function must be declared static to fit into Instruction RAM of the ESP32. Also, DMA
 * descriptor must be already configured!
 */
void my_sendDataI2S(i2s_dev_t *_i2sDev, volatile lldesc_s *_dmaDecs) {
  // Stop any on-going transmission (just in case).
  _i2sDev->out_link.stop  = 1;
  _i2sDev->out_link.start = 0;
  _i2sDev->conf.tx_start  = 0;

  // Reset the FIFO.
  _i2sDev->conf.tx_fifo_reset = 1;
  _i2sDev->conf.tx_fifo_reset = 0;

  // Reset the I2S DMA Controller.
  _i2sDev->lc_conf.out_rst = 1;
  _i2sDev->lc_conf.out_rst = 0;

  // Reset I2S TX module.
  _i2sDev->conf.tx_reset = 1;
  _i2sDev->conf.tx_reset = 0;

  // Setup a DMA descriptor.
  _i2sDev->lc_conf.val   = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN;
  _i2sDev->out_link.addr = (uint32_t)(_dmaDecs) & 0x000FFFFF;

  // Start sending the data
  _i2sDev->out_link.start = 1;

  // Pull SPH low -> Start pushing data into the row of EPD.
  SPH_CLEAR;

  // Set CKV to HIGH.
  CKV_SET;

  // Start sending I2S data out.
  _i2sDev->conf.tx_start = 1;

  while (!_i2sDev->int_raw.out_total_eof)
    ;

  SPH_SET;

  // Clear the interrupt flags and stop the transmission.
  _i2sDev->int_clr.val    = _i2sDev->int_raw.val;
  _i2sDev->out_link.stop  = 1;
  _i2sDev->out_link.start = 0;
}

void my_setI2S1pin(uint32_t _pin, uint32_t _function, uint32_t _inv) {
  // Check if valid pin is selected
  if (_pin > 39) return;

  // Setup GPIO pin for I2S output
  gpio_set_direction((gpio_num_t)_pin, GPIO_MODE_OUTPUT);
  
  // Set drive strength to maximum (3 = 40mA)
  gpio_set_drive_capability((gpio_num_t)_pin, GPIO_DRIVE_CAP_3);
  
  // Connect the I2S output signal to this pin through GPIO matrix
  esp_rom_gpio_connect_out_signal(_pin, _function, _inv, false);
}

void I2SComms::init_lldesc() {
  if (lldesc != nullptr) {
    lldesc->size         = line_buffer_size; // Buffer size
    lldesc->length       = line_buffer_size; // Number of bytes to transfer
    lldesc->offset       = 0;                // Start transfer with first buffer byte
    lldesc->sosf         = 1;
    lldesc->eof          = 1;           // Only one block at a time (end of list)
    lldesc->owner        = 1;           // The allowed operator is the DMA controller
    lldesc->buf          = line_buffer; // Buffer address
    lldesc->qe.stqe_next = 0;           // Next descriptor (none)
  }
}

#undef __I2S_COMMS__

#endif