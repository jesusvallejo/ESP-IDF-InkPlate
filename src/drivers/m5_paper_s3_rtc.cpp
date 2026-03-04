/*
m5_paper_s3_rtc.cpp
BM8563 Real-Time Clock Implementation for M5 Paper S3

Specifications:
- I2C Address: 0x51
- Operating voltage: 2.5V - 3.6V (3.3V nominal)
- Battery-backed time keeping
- Timer capability with configurable frequency
- Alarm with minute resolution

This code is released under the GNU Lesser General Public License v3.0
*/

#define _GNU_SOURCE  // Enable timegm() on Linux

#include "m5_paper_s3_rtc.hpp"
#include "logging.hpp"
#include <cstring>

static constexpr char const * TAG = "BM8563RTC";

BM8563RTC::BM8563RTC()
  : wire_device(nullptr), present(false),
    second(0), minute(0), hour(0), day(1),
    week_day(WeekDay::SUN), month(1), year(2024)
{
  LOG_I("BM8563RTC constructor");
}

uint8_t BM8563RTC::dec_to_bcd(uint8_t val)
{
  return ((val / 10) << 4) + (val % 10);
}

uint8_t BM8563RTC::bcd_to_dec(uint8_t val)
{
  return ((val >> 4) * 10) + (val & 0x0F);
}

bool BM8563RTC::setup()
{
  LOG_I("BM8563RTC::setup()");

  wire_device = new WireDevice(RTC_ADDR);
  present = (wire_device != nullptr) && wire_device->is_initialized();

  if (!present) {
    LOG_E("BM8563 RTC not found at address 0x%02X", RTC_ADDR);
    return false;
  }

  LOG_I("BM8563 RTC found at address 0x%02X", RTC_ADDR);

  // Read status registers to verify chip is working
  uint8_t ctrl1 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS1));
  uint8_t ctrl2 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS2));
  LOG_D("BM8563 CTRL1: 0x%02X, CTRL2: 0x%02X", ctrl1, ctrl2);

  // Clear any existing alarm or timer flags
  if (ctrl2 & CTRL2_AF_BIT) {
    LOG_D("Clearing alarm flag");
    clear_alarm_flag();
  }
  if (ctrl2 & CTRL2_TF_BIT) {
    LOG_D("Clearing timer flag");
    clear_timer_flag();
  }

  // Start the oscillator if not running
  if (ctrl1 & CTRL1_STOP_BIT) {
    LOG_I("RTC oscillator stopped, starting it");
    start();
  }

  return true;
}

void BM8563RTC::reset()
{
  LOG_I("BM8563RTC::reset()");

  if (!present) return;

  // Set software reset bit (not a standard BM8563 operation)
  // Instead, stop and restart the oscillator
  stop();
  start();

  // Set default time to 2024/01/01 00:00:00
  set_date_time(2024, 1, 1, 0, 0, 0, WeekDay::MON);
}

void BM8563RTC::start()
{
  LOG_I("BM8563RTC::start()");

  if (!present) return;

  // Clear the STOP bit in CTRL_STATUS1
  uint8_t ctrl1 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS1));
  ctrl1 &= ~CTRL1_STOP_BIT;
  wire_device->cmd_write(static_cast<uint8_t>(Reg::CTRL_STATUS1), ctrl1);

  LOG_D("RTC oscillator started");
}

void BM8563RTC::stop()
{
  LOG_I("BM8563RTC::stop()");

  if (!present) return;

  // Set the STOP bit in CTRL_STATUS1
  uint8_t ctrl1 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS1));
  ctrl1 |= CTRL1_STOP_BIT;
  wire_device->cmd_write(static_cast<uint8_t>(Reg::CTRL_STATUS1), ctrl1);

  LOG_D("RTC oscillator stopped");
}

void BM8563RTC::read_date_time()
{
  if (!present) return;

  uint8_t data[7];
  wire_device->cmd_read(static_cast<uint8_t>(Reg::SECONDS), data, sizeof(data));

  second = bcd_to_dec(data[0] & SECONDS_MASK);
  minute = bcd_to_dec(data[1]);
  hour = bcd_to_dec(data[2] & HOUR_MASK);
  day = bcd_to_dec(data[3] & DAY_MASK);
  week_day = static_cast<WeekDay>(bcd_to_dec(data[4]));
  month = bcd_to_dec(data[5]);
  year = bcd_to_dec(data[6]) + 2000;

  LOG_D("Read time: %04d-%02d-%02d %02d:%02d:%02d (DOW: %d)", 
        year, month, day, hour, minute, second, static_cast<uint8_t>(week_day));
}

void BM8563RTC::write_date_time()
{
  if (!present) return;

  wire_device->cmd_write(static_cast<uint8_t>(Reg::SECONDS), dec_to_bcd(second & SECONDS_MASK));
  wire_device->cmd_write(static_cast<uint8_t>(Reg::MINUTES), dec_to_bcd(minute));
  wire_device->cmd_write(static_cast<uint8_t>(Reg::HOURS), dec_to_bcd(hour & HOUR_MASK));
  wire_device->cmd_write(static_cast<uint8_t>(Reg::DAY), dec_to_bcd(day & DAY_MASK));
  wire_device->cmd_write(static_cast<uint8_t>(Reg::WEEKDAY), static_cast<uint8_t>(week_day));
  wire_device->cmd_write(static_cast<uint8_t>(Reg::MONTH), dec_to_bcd(month));
  wire_device->cmd_write(static_cast<uint8_t>(Reg::YEAR), dec_to_bcd(year - 2000));

  LOG_D("Write time: %04d-%02d-%02d %02d:%02d:%02d (DOW: %d)",
        year, month, day, hour, minute, second, static_cast<uint8_t>(week_day));
}

void BM8563RTC::set_date_time(uint16_t y, uint8_t m, uint8_t d,
                              uint8_t h, uint8_t mm, uint8_t s,
                              WeekDay wd)
{
  if (!present) return;

  year = y;
  month = m;
  day = d;
  hour = h;
  minute = mm;
  second = s;
  week_day = wd;

  write_date_time();
}

void BM8563RTC::set_date_time(const time_t * t)
{
  if (!present) return;

  struct tm time;
  if (gmtime_r(t, &time) == nullptr) {
    LOG_E("Failed to convert time_t to struct tm");
    return;
  }

  uint16_t year_val = time.tm_year + 1900;
  if ((year_val < 2000) || (year_val > 2099)) {
    LOG_E("Time year %d out of valid range (2000-2099)", year_val);
    return;
  }

  set_date_time(
    year_val,
    static_cast<uint8_t>(time.tm_mon + 1),
    static_cast<uint8_t>(time.tm_mday),
    static_cast<uint8_t>(time.tm_hour),
    static_cast<uint8_t>(time.tm_min),
    static_cast<uint8_t>(time.tm_sec),
    static_cast<WeekDay>(time.tm_wday));
}

void BM8563RTC::get_date_time(uint16_t & y, uint8_t & m, uint8_t & d,
                              uint8_t & h, uint8_t & mm, uint8_t & s,
                              WeekDay & wd)
{
  if (!present) return;

  read_date_time();

  y = year;
  m = month;
  d = day;
  h = hour;
  mm = minute;
  s = second;
  wd = week_day;
}

void BM8563RTC::get_date_time(time_t * t)
{
  if (!present) return;

  struct tm time;
  memset(&time, 0, sizeof(time));

  read_date_time();

  time.tm_year = year - 1900;
  time.tm_mon = month - 1;
  time.tm_mday = day;
  time.tm_hour = hour;
  time.tm_min = minute;
  time.tm_sec = second;
  time.tm_wday = static_cast<uint8_t>(week_day);

  *t = timegm(&time);

  LOG_D("Time as Unix timestamp: %ld", *t);
}

void BM8563RTC::set_alarm(uint8_t minutes, uint8_t hours, uint8_t days, uint8_t weekdays)
{
  if (!present) return;

  LOG_D("Setting alarm: min=%d, hour=%d, day=%d, dow=%d", minutes, hours, days, weekdays);

  wire_device->cmd_write(static_cast<uint8_t>(Reg::MINUTE_ALARM), dec_to_bcd(minutes));
  wire_device->cmd_write(static_cast<uint8_t>(Reg::HOUR_ALARM), dec_to_bcd(hours));
  wire_device->cmd_write(static_cast<uint8_t>(Reg::DAY_ALARM), dec_to_bcd(days));
  wire_device->cmd_write(static_cast<uint8_t>(Reg::WEEKDAY_ALARM), weekdays);
}

void BM8563RTC::enable_alarm()
{
  if (!present) return;

  LOG_I("Enabling RTC alarm");

  uint8_t ctrl2 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS2));
  ctrl2 |= CTRL2_AIE_BIT;
  wire_device->cmd_write(static_cast<uint8_t>(Reg::CTRL_STATUS2), ctrl2);
}

void BM8563RTC::disable_alarm()
{
  if (!present) return;

  LOG_I("Disabling RTC alarm");

  uint8_t ctrl2 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS2));
  ctrl2 &= ~CTRL2_AIE_BIT;
  wire_device->cmd_write(static_cast<uint8_t>(Reg::CTRL_STATUS2), ctrl2);
}

void BM8563RTC::clear_alarm_flag()
{
  if (!present) return;

  LOG_D("Clearing RTC alarm flag");

  uint8_t ctrl2 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS2));
  ctrl2 &= ~CTRL2_AF_BIT;
  wire_device->cmd_write(static_cast<uint8_t>(Reg::CTRL_STATUS2), ctrl2);
}

void BM8563RTC::set_timer(TimerFrequency frequency, uint8_t value)
{
  if (!present) return;

  LOG_D("Setting timer: frequency=%d, value=%d", static_cast<uint8_t>(frequency), value);

  uint8_t ctrl = static_cast<uint8_t>(frequency) & 0x03;
  wire_device->cmd_write(static_cast<uint8_t>(Reg::TIMER_CTRL), ctrl);
  wire_device->cmd_write(static_cast<uint8_t>(Reg::TIMER_VALUE), value);
}

void BM8563RTC::enable_timer()
{
  if (!present) return;

  LOG_I("Enabling RTC timer");

  uint8_t ctrl2 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS2));
  ctrl2 |= CTRL2_TIE_BIT;
  wire_device->cmd_write(static_cast<uint8_t>(Reg::CTRL_STATUS2), ctrl2);
}

void BM8563RTC::disable_timer()
{
  if (!present) return;

  LOG_I("Disabling RTC timer");

  uint8_t ctrl2 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS2));
  ctrl2 &= ~CTRL2_TIE_BIT;
  wire_device->cmd_write(static_cast<uint8_t>(Reg::CTRL_STATUS2), ctrl2);
}

void BM8563RTC::clear_timer_flag()
{
  if (!present) return;

  LOG_D("Clearing RTC timer flag");

  uint8_t ctrl2 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS2));
  ctrl2 &= ~CTRL2_TF_BIT;
  wire_device->cmd_write(static_cast<uint8_t>(Reg::CTRL_STATUS2), ctrl2);
}

bool BM8563RTC::is_alarm_flag_set()
{
  if (!present) return false;

  uint8_t ctrl2 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS2));
  return (ctrl2 & CTRL2_AF_BIT) != 0;
}

bool BM8563RTC::is_timer_flag_set()
{
  if (!present) return false;

  uint8_t ctrl2 = wire_device->cmd_read(static_cast<uint8_t>(Reg::CTRL_STATUS2));
  return (ctrl2 & CTRL2_TF_BIT) != 0;
}
