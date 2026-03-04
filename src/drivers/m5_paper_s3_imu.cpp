#if M5_PAPER_S3

#include "m5_paper_s3_imu.hpp"
#include "logging.hpp"
#include <cmath>

BMI270IMU::BMI270IMU() 
  : wire_device(nullptr), present(false), device_address(IMU_ADDR_PRIMARY),
    accel_range(AccelRange::RANGE_2G), gyro_range(GyroRange::RANGE_250) 
{
}

bool BMI270IMU::setup()
{
  LOG_I("BMI270IMU::setup() - Initializing 6-DOF accelerometer/gyroscope");

  // Try primary address first, then alternate
  for (uint8_t i = 0; i < 2; i++) {
    device_address = (i == 0) ? IMU_ADDR_PRIMARY : IMU_ADDR_ALTERNATE;
    
    wire_device = Wire::get_device(device_address, 100000);
    if (!wire_device) {
      LOG_W("Could not get Wire device at 0x%02X", device_address);
      continue;
    }

    // Read chip ID to verify device
    uint8_t chip_id = read_register(static_cast<uint8_t>(Reg::CHIP_ID));
    LOG_D("BMI270 chip ID at 0x%02X: 0x%02X (expected 0x%02X)", 
          device_address, chip_id, BMI270_CHIP_ID);

    if (chip_id != BMI270_CHIP_ID) {
      LOG_W("Invalid chip ID at 0x%02X", device_address);
      continue;
    }

    // Soft reset
    LOG_D("Performing soft reset of BMI270");
    write_register(static_cast<uint8_t>(Reg::CMD), CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Enable accelerometer and gyroscope
    // PWR_CTRL: bit 7 = accel_en, bit 6 = gyro_en, bit 5 = mag_en
    uint8_t pwr_ctrl = (1 << 7) | (1 << 6);  // Enable accel and gyro
    write_register(static_cast<uint8_t>(Reg::PWR_CTRL), pwr_ctrl);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Set ranges
    set_accel_range(AccelRange::RANGE_2G);
    set_gyro_range(GyroRange::RANGE_250);

    // Enable data ready interrupt on INT1
    uint8_t int_map_data = 0x04;  // Map data ready to INT1
    write_register(static_cast<uint8_t>(Reg::INT_MAP_DATA), int_map_data);

    present = true;
    LOG_I("BMI270 initialized successfully at address 0x%02X", device_address);
    return true;
  }

  LOG_E("BMI270 not found on I2C bus");
  present = false;
  return false;
}

uint8_t BMI270IMU::read_register(uint8_t reg)
{
  if (!wire_device) return 0;
  
  uint8_t data = 0;
  Wire::enter();
  wire_device->read_register(reg, &data, 1);
  Wire::leave();
  return data;
}

void BMI270IMU::write_register(uint8_t reg, uint8_t value)
{
  if (!wire_device) return;
  
  Wire::enter();
  wire_device->write_register(reg, &value, 1);
  Wire::leave();
}

void BMI270IMU::read_registers(uint8_t reg, uint8_t * data, uint16_t len)
{
  if (!wire_device) return;
  
  Wire::enter();
  wire_device->read_register(reg, data, len);
  Wire::leave();
}

void BMI270IMU::set_accel_range(AccelRange range)
{
  accel_range = range;
  write_register(static_cast<uint8_t>(Reg::ACCEL_RANGE), static_cast<uint8_t>(range));
  LOG_D("Accelerometer range set to %d", static_cast<uint8_t>(range));
}

void BMI270IMU::set_gyro_range(GyroRange range)
{
  gyro_range = range;
  write_register(static_cast<uint8_t>(Reg::GYRO_RANGE), static_cast<uint8_t>(range));
  LOG_D("Gyroscope range set to %d", static_cast<uint8_t>(range));
}

Vector3D BMI270IMU::read_accel()
{
  Vector3D accel = {0, 0, 0};
  if (!present) return accel;

  uint8_t data[6];
  read_registers(static_cast<uint8_t>(Reg::ACCEL_DATA_X), data, 6);

  // Convert raw data to 16-bit values (little-endian)
  int16_t accel_x = (data[1] << 8) | data[0];
  int16_t accel_y = (data[3] << 8) | data[2];
  int16_t accel_z = (data[5] << 8) | data[4];

  // Scale to m/s² based on range
  float scale = 0;
  switch (accel_range) {
    case AccelRange::RANGE_2G:   scale = 9.81f * 2.0f / 32768.0f; break;
    case AccelRange::RANGE_4G:   scale = 9.81f * 4.0f / 32768.0f; break;
    case AccelRange::RANGE_8G:   scale = 9.81f * 8.0f / 32768.0f; break;
    case AccelRange::RANGE_16G:  scale = 9.81f * 16.0f / 32768.0f; break;
  }

  accel.x = accel_x * scale;
  accel.y = accel_y * scale;
  accel.z = accel_z * scale;

  return accel;
}

Vector3D BMI270IMU::read_gyro()
{
  Vector3D gyro = {0, 0, 0};
  if (!present) return gyro;

  uint8_t data[6];
  read_registers(static_cast<uint8_t>(Reg::GYRO_DATA_X), data, 6);

  // Convert raw data to 16-bit values (little-endian)
  int16_t gyro_x = (data[1] << 8) | data[0];
  int16_t gyro_y = (data[3] << 8) | data[2];
  int16_t gyro_z = (data[5] << 8) | data[4];

  // Scale to rad/s based on range
  float scale = 0;
  float deg_per_sec = 0;
  switch (gyro_range) {
    case GyroRange::RANGE_125:  deg_per_sec = 125.0f; break;
    case GyroRange::RANGE_250:  deg_per_sec = 250.0f; break;
    case GyroRange::RANGE_500:  deg_per_sec = 500.0f; break;
    case GyroRange::RANGE_1000: deg_per_sec = 1000.0f; break;
    case GyroRange::RANGE_2000: deg_per_sec = 2000.0f; break;
  }
  scale = deg_per_sec / 32768.0f * 3.14159265f / 180.0f;  // Convert to rad/s

  gyro.x = gyro_x * scale;
  gyro.y = gyro_y * scale;
  gyro.z = gyro_z * scale;

  return gyro;
}

float BMI270IMU::read_temperature()
{
  if (!present) return 0;

  uint8_t data = read_register(static_cast<uint8_t>(Reg::TEMP_DATA));
  
  // Temperature: 23°C @ 0x00, 1°C per LSB
  float temperature = 23.0f + (static_cast<int8_t>(data) * 1.0f);
  return temperature;
}

IMUData BMI270IMU::read_all()
{
  IMUData data;
  data.accel = read_accel();
  data.gyro = read_gyro();
  data.temperature = read_temperature();
  return data;
}

void BMI270IMU::enable_motion_interrupt(uint16_t threshold)
{
  if (!present) return;

  LOG_D("Enabling motion interrupt with threshold %u mg", threshold);

  // Configure motion detection threshold (simplified)
  // Typical: threshold = value / 250 * 256 for 2g range
  // This is a simplified version - full implementation would need more registers
  
  // Enable INT1 output for motion
  uint8_t int_ctrl = read_register(static_cast<uint8_t>(Reg::INT_CTRL));
  int_ctrl |= 0x04;  // Enable INT1
  write_register(static_cast<uint8_t>(Reg::INT_CTRL), int_ctrl);
}

void BMI270IMU::disable_motion_interrupt()
{
  if (!present) return;

  LOG_D("Disabling motion interrupt");

  uint8_t int_ctrl = read_register(static_cast<uint8_t>(Reg::INT_CTRL));
  int_ctrl &= ~0x04;  // Disable INT1
  write_register(static_cast<uint8_t>(Reg::INT_CTRL), int_ctrl);
}

void BMI270IMU::set_accel_power_mode(bool active)
{
  if (!present) return;

  uint8_t pwr_ctrl = read_register(static_cast<uint8_t>(Reg::PWR_CTRL));
  
  if (active) {
    pwr_ctrl |= 0x80;  // Set bit 7 to enable accelerometer
    LOG_D("Accelerometer active mode enabled");
  } else {
    pwr_ctrl &= ~0x80;  // Clear bit 7 for low-power mode
    LOG_D("Accelerometer low-power mode enabled");
  }
  
  write_register(static_cast<uint8_t>(Reg::PWR_CTRL), pwr_ctrl);
}

void BMI270IMU::set_gyro_power_mode(bool enabled)
{
  if (!present) return;

  uint8_t pwr_ctrl = read_register(static_cast<uint8_t>(Reg::PWR_CTRL));
  
  if (enabled) {
    pwr_ctrl |= 0x40;  // Set bit 6 to enable gyroscope
    LOG_D("Gyroscope enabled");
  } else {
    pwr_ctrl &= ~0x40;  // Clear bit 6 to disable gyroscope
    LOG_D("Gyroscope disabled");
  }
  
  write_register(static_cast<uint8_t>(Reg::PWR_CTRL), pwr_ctrl);
}

#endif // M5_PAPER_S3
