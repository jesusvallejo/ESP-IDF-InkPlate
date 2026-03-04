#if M5_PAPER_S3

#include "opds_config.hpp"
#include "esp_log.h"
#include <fstream>
#include <sstream>
#include <algorithm>

static const char* TAG = "OPDS_Config";

OPDSConfig::OPDSConfig() : is_loaded(false)
{
  config.use_https = true;
}

OPDSConfig::~OPDSConfig()
{
}

bool OPDSConfig::load()
{
  std::string config_path = get_config_path();
  std::ifstream file(config_path);
  
  if (!file.is_open()) {
    ESP_LOGI(TAG, "No existing config file, creating new one");
    return true;  // Not an error - first time setup
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();

  std::string json_str = buffer.str();
  if (parse_json(json_str)) {
    is_loaded = true;
    ESP_LOGI(TAG, "Configuration loaded from %s", config_path.c_str());
    return true;
  }

  ESP_LOGE(TAG, "Failed to parse configuration file");
  return false;
}

bool OPDSConfig::save()
{
  if (!ensure_books_dir()) {
    ESP_LOGE(TAG, "Failed to ensure /books directory exists");
    return false;
  }

  std::string config_path = get_config_path();
  std::string json_str = to_json();

  std::ofstream file(config_path);
  if (!file.is_open()) {
    ESP_LOGE(TAG, "Failed to open config file for writing: %s", config_path.c_str());
    return false;
  }

  file << json_str;
  file.close();

  ESP_LOGI(TAG, "Configuration saved to %s", config_path.c_str());
  return true;
}

bool OPDSConfig::exists()
{
  std::ifstream file(get_config_path());
  return file.good();
}

void OPDSConfig::set_config(const Config& cfg)
{
  config = cfg;
}

void OPDSConfig::clear()
{
  config.url.clear();
  config.username.clear();
  config.password.clear();
  config.use_https = true;
  is_loaded = false;
}

bool OPDSConfig::ensure_books_dir()
{
  // Check if /sdcard/books exists, if not try to create
  // This is a simplified version - actual implementation would use FatFS
  std::string books_dir = "/sdcard/books";
  
  // Try to open a file in the directory to verify it exists
  std::string test_file = books_dir + "/.test";
  std::ofstream f(test_file);
  if (f.is_open()) {
    f.close();
    // Remove test file
    remove(test_file.c_str());
    return true;
  }

  ESP_LOGW(TAG, "Failed to verify /books directory");
  return false;
}

bool OPDSConfig::parse_json(const std::string& json_str)
{
  // Simple JSON parser for OPDS config
  // Format: {"url":"...", "username":"...", "password":"...", "use_https":true}

  try {
    // Extract URL
    size_t url_pos = json_str.find("\"url\":");
    if (url_pos != std::string::npos) {
      size_t start = json_str.find("\"", url_pos + 6);
      size_t end = json_str.find("\"", start + 1);
      if (start != std::string::npos && end != std::string::npos) {
        config.url = json_str.substr(start + 1, end - start - 1);
      }
    }

    // Extract username
    size_t user_pos = json_str.find("\"username\":");
    if (user_pos != std::string::npos) {
      size_t start = json_str.find("\"", user_pos + 11);
      size_t end = json_str.find("\"", start + 1);
      if (start != std::string::npos && end != std::string::npos) {
        config.username = json_str.substr(start + 1, end - start - 1);
      }
    }

    // Extract password
    size_t pass_pos = json_str.find("\"password\":");
    if (pass_pos != std::string::npos) {
      size_t start = json_str.find("\"", pass_pos + 11);
      size_t end = json_str.find("\"", start + 1);
      if (start != std::string::npos && end != std::string::npos) {
        config.password = json_str.substr(start + 1, end - start - 1);
      }
    }

    // Extract use_https
    size_t https_pos = json_str.find("\"use_https\":");
    if (https_pos != std::string::npos) {
      size_t val_start = json_str.find_first_not_of(" \t\n\r", https_pos + 12);
      std::string val_str = json_str.substr(val_start, 5);
      config.use_https = (val_str.find("true") != std::string::npos);
    }

    return !config.url.empty();
  } catch (...) {
    ESP_LOGE(TAG, "Exception while parsing JSON");
    return false;
  }
}

std::string OPDSConfig::to_json() const
{
  // Build simple JSON representation
  std::stringstream json;
  json << "{"
       << "\"url\":\"" << config.url << "\","
       << "\"username\":\"" << config.username << "\","
       << "\"password\":\"" << config.password << "\","
       << "\"use_https\":" << (config.use_https ? "true" : "false")
       << "}";
  return json.str();
}

#endif // M5_PAPER_S3
