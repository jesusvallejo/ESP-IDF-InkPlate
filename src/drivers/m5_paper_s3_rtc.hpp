#pragma once

#include <cinttypes>
#include "non_copyable.hpp"
#include "wire.hpp"
#include <ctime>

/**
 * BM8563 Real-Time Clock Driver for M5 Paper S3
 * 
 * I2C Address: 0x51
 * Features:
 * - Battery-backed date/time keeping
 * - Timer with configurable frequency
 * - Alarm with minute resolution
 * - Temperature-compensated oscillator
 */
class BM8563RTC : NonCopyable {
  public:
    enum class WeekDay : uint8_t {
      SUN = 0,
      MON = 1,
      TUE = 2,
      WED = 3,
      THU = 4,
      FRI = 5,
      SAT = 6
    };

    enum class TimerFrequency : uint8_t {
      FREQ_4096_HZ    = 0x00,  // 122 microseconds
      FREQ_64_HZ      = 0x01,  // 15.625 milliseconds
      FREQ_1_HZ       = 0x02,  // 1 second
      FREQ_1_60_HZ    = 0x03   // 1/60 second
    };

  private:
    static constexpr char const * TAG = "BM8563RTC";
    static constexpr uint8_t RTC_ADDR = 0x51;

    WireDevice * wire_device;
    bool present;

    // BM8563 Register Addresses
    enum class Reg : uint8_t {
      CTRL_STATUS1 = 0x00,
      CTRL_STATUS2 = 0x01,
      SECONDS      = 0x02,
      MINUTES      = 0x03,
      HOURS        = 0x04,
      DAY          = 0x05,
      WEEKDAY      = 0x06,
      MONTH        = 0x07,
      YEAR         = 0x08,
      MINUTE_ALARM = 0x09,
      HOUR_ALARM   = 0x0A,
      DAY_ALARM    = 0x0B,
      WEEKDAY_ALARM= 0x0C,
      TIMER_CTRL   = 0x0E,
      TIMER_VALUE  = 0x0F
    };

    // Control/Status bit masks
    static constexpr uint8_t CTRL1_TEST_BIT = 0x80;
    static constexpr uint8_t CTRL1_STOP_BIT = 0x20;
    static constexpr uint8_t CTRL1_12_24_BIT = 0x04;
    static constexpr uint8_t CTRL2_AF_BIT = 0x40;    // Alarm flag
    static constexpr uint8_t CTRL2_TF_BIT = 0x08;    // Timer flag
    static constexpr uint8_t CTRL2_AIE_BIT = 0x02;   // Alarm interrupt enable
    static constexpr uint8_t CTRL2_TIE_BIT = 0x01;   // Timer interrupt enable
    
    static constexpr uint8_t SECONDS_MASK = 0x7F;
    static constexpr uint8_t DAY_MASK = 0x3F;
    static constexpr uint8_t HOUR_MASK = 0x3F;

    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    WeekDay week_day;
    uint8_t month;
    uint16_t year;

    // Helper functions
    uint8_t dec_to_bcd(uint8_t value);
    uint8_t bcd_to_dec(uint8_t value);
    void read_date_time();
    void write_date_time();

  public:
    BM8563RTC();
    ~BM8563RTC() = default;

    /**
     * Initialize the RTC
     * @return true if RTC detected and initialized, false otherwise
     */
    bool setup();

    /** Reset the RTC to default state */
    void reset();

    /** Start the RTC oscillator */
    void start();

    /** Stop the RTC oscillator (preserves time) */
    void stop();

    /**
     * Set date and time
     * @param y Year (2000-2099)
     * @param m Month (1-12)
     * @param d Day (1-31)
     * @param h Hour (0-23)
     * @param mm Minute (0-59)
     * @param s Second (0-59)
     * @param wd Weekday
     */
    void set_date_time(uint16_t y, uint8_t m, uint8_t d,
                       uint8_t h, uint8_t mm, uint8_t s,
                       WeekDay wd);

    /**
     * Set date and time from Unix timestamp
     * @param t Pointer to time_t value (must represent time >= 2000/01/01 and <= 2099/12/31)
     */
    void set_date_time(const time_t * t);

    /**
     * Get current date and time
     * @param y Year (output)
     * @param m Month (output, 1-12)
     * @param d Day (output, 1-31)
     * @param h Hour (output, 0-23)
     * @param mm Minute (output, 0-59)
     * @param s Second (output, 0-59)
     * @param wd Weekday (output)
     */
    void get_date_time(uint16_t & y, uint8_t & m, uint8_t & d,
                       uint8_t & h, uint8_t & mm, uint8_t & s,
                       WeekDay & wd);

    /**
     * Get current date and time as Unix timestamp
     * @param t Pointer to time_t variable (output)
     */
    void get_date_time(time_t * t);

    /**
     * Set alarm time
     * @param minutes Minute value (0-59, 0x80 = no alarm)
     * @param hours Hour value (0-23, 0x80 = no alarm)
     * @param days Day value (1-31, 0x80 = no alarm)
     * @param weekdays Weekday (0-6, 0x80 = no alarm)
     */
    void set_alarm(uint8_t minutes, uint8_t hours, uint8_t days, uint8_t weekdays);

    /** Enable alarm interrupt */
    void enable_alarm();

    /** Disable alarm interrupt */
    void disable_alarm();

    /** Clear alarm flag */
    void clear_alarm_flag();

    /**
     * Set timer
     * @param frequency Timer frequency
     * @param value Timer count value
     */
    void set_timer(TimerFrequency frequency, uint8_t value);

    /** Enable timer interrupt */
    void enable_timer();

    /** Disable timer interrupt */
    void disable_timer();

    /** Clear timer flag */
    void clear_timer_flag();

    /** Check if RTC is present on I2C bus */
    inline bool is_present() const { return present; }

    /** Check if alarm flag is set */
    bool is_alarm_flag_set();

    /** Check if timer flag is set */
    bool is_timer_flag_set();
};
