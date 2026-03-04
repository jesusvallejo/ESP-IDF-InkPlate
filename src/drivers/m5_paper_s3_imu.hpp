#pragma once

#include <cinttypes>
#include "non_copyable.hpp"
#include "wire.hpp"

/**
 * BMI270 6-DOF IMU Driver for M5 Paper S3
 * 
 * I2C Address: 0x68 (default) or 0x69 (alternate)
 * Features:
 * - 3-axis accelerometer (±2g to ±16g)
 * - 3-axis gyroscope (±125° to ±2000°/s)
 * - Temperature sensor
 * - Low power mode support
 * - Interrupt outputs
 */
class BMI270IMU : NonCopyable {
  public:
    enum class AccelRange : uint8_t {
      RANGE_2G   = 0x00,   // ±2g
      RANGE_4G   = 0x01,   // ±4g
      RANGE_8G   = 0x02,   // ±8g
      RANGE_16G  = 0x03    // ±16g
    };

    enum class GyroRange : uint8_t {
      RANGE_125  = 0x04,   // ±125°/s
      RANGE_250  = 0x03,   // ±250°/s
      RANGE_500  = 0x02,   // ±500°/s
      RANGE_1000 = 0x01,   // ±1000°/s
      RANGE_2000 = 0x00    // ±2000°/s
    };

    struct Vector3D {
      float x;
      float y;
      float z;
    };

    struct IMUData {
      Vector3D accel;         // Acceleration in m/s²
      Vector3D gyro;          // Angular velocity in rad/s
      float temperature;      // Temperature in °C
    };

  private:
    static constexpr char const * TAG = "BMI270IMU";
    static constexpr uint8_t IMU_ADDR_PRIMARY = 0x68;
    static constexpr uint8_t IMU_ADDR_ALTERNATE = 0x69;

    WireDevice * wire_device;
    bool present;
    uint8_t device_address;

    // BMI270 Register Addresses
    enum class Reg : uint8_t {
      CHIP_ID        = 0x00,
      ERR_REG        = 0x02,
      STATUS         = 0x03,
      ACCEL_DATA_X   = 0x0C,
      ACCEL_DATA_Y   = 0x0E,
      ACCEL_DATA_Z   = 0x10,
      GYRO_DATA_X    = 0x12,
      GYRO_DATA_Y    = 0x14,
      GYRO_DATA_Z    = 0x16,
      SENSOR_TIME    = 0x18,
      EVENT          = 0x1B,
      INT_STATUS_0   = 0x1C,
      INT_STATUS_1   = 0x1D,
      SENSOR_STATUS  = 0x21,
      TEMP_DATA      = 0x22,
      ACCEL_CONFIG   = 0x40,
      ACCEL_RANGE    = 0x41,
      GYRO_CONFIG    = 0x42,
      GYRO_RANGE     = 0x43,
      INT_MAP_DATA   = 0x58,
      DATA_SYNC      = 0x59,
      INIT_CTRL      = 0x59,
      INIT_DATA_0    = 0x5E,
      INIT_DATA_1    = 0x5F,
      INT_MAP_1      = 0x56,
      INT_MAP_2      = 0x57,
      INT_CTRL       = 0x58,
      INT_CONFIG     = 0x5F,
      PWR_CONF       = 0x7C,
      PWR_CTRL       = 0x7D,
      CMD            = 0x7E
    };

    // Commands
    static constexpr uint8_t CMD_SOFT_RESET = 0xB6;

    // Power modes
    static constexpr uint8_t ACCEL_PM_SUSPEND = 0x00;
    static constexpr uint8_t ACCEL_PM_ACTIVE = 0x01;
    static constexpr uint8_t ACCEL_PM_LOW_POWER = 0x03;
    static constexpr uint8_t GYRO_PM_SUSPEND = 0x00;
    static constexpr uint8_t GYRO_PM_NORMAL = 0x01;

    // Chip ID
    static constexpr uint8_t BMI270_CHIP_ID = 0x71;

    uint8_t read_register(uint8_t reg);
    void write_register(uint8_t reg, uint8_t value);
    void read_registers(uint8_t reg, uint8_t * data, uint16_t len);
    
    void set_accel_range(AccelRange range);
    void set_gyro_range(GyroRange range);
    AccelRange accel_range;
    GyroRange gyro_range;

  public:
    BMI270IMU();
    ~BMI270IMU() = default;

    /**
     * Initialize the IMU
     * @return true if IMU detected and initialized, false otherwise
     */
    bool setup();

    /**
     * Read accelerometer data (m/s²)
     * @return Acceleration vector
     */
    Vector3D read_accel();

    /**
     * Read gyroscope data (rad/s)
     * @return Angular velocity vector
     */
    Vector3D read_gyro();

    /**
     * Read temperature (°C)
     * @return Temperature value
     */
    float read_temperature();

    /**
     * Read all IMU data at once
     * @return IMU data structure
     */
    IMUData read_all();

    /**
     * Enable motion interrupt
     * @param threshold Acceleration threshold in mg
     */
    void enable_motion_interrupt(uint16_t threshold);

    /**
     * Disable motion interrupt
     */
    void disable_motion_interrupt();

    /**
     * Set power mode for accelerometer
     * @param active true for active mode, false for low-power
     */
    void set_accel_power_mode(bool active);

    /**
     * Set power mode for gyroscope
     * @param enabled true to enable gyroscope
     */
    void set_gyro_power_mode(bool enabled);

    /**
     * Check if IMU is present and initialized
     */
    inline bool is_present() const { return present; }

    /**
     * Get device I2C address
     */
    inline uint8_t get_address() const { return device_address; }
};
