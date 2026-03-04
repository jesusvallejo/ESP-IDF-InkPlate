#pragma once

#if M5_PAPER_S3

#include <string>
#include <cstdint>

/**
 * @brief OPDS Configuration Manager
 * 
 * Manages OPDS server configuration (URL, credentials) stored on SD card as JSON.
 * Default config location: /books/.opds_config.json
 */
class OPDSConfig
{
  public:
    struct Config {
      std::string url;        // OPDS server URL (e.g., https://booklore.com/opds)
      std::string username;   // Optional username for authentication
      std::string password;   // Optional password for authentication
      bool use_https;         // Use HTTPS instead of HTTP
    };

    OPDSConfig();
    ~OPDSConfig();

    /**
     * @brief Load configuration from SD card
     * @return true if loaded successfully (existing config or new)
     */
    bool load();

    /**
     * @brief Save current configuration to SD card
     * @return true if saved successfully
     */
    bool save();

    /**
     * @brief Check if configuration exists
     * @return true if config file exists on SD card
     */
    bool exists();

    /**
     * @brief Get current configuration
     * @return Reference to config struct
     */
    const Config& get_config() const { return config; }

    /**
     * @brief Update configuration
     * @param cfg New configuration
     */
    void set_config(const Config& cfg);

    /**
     * @brief Clear all configuration
     */
    void clear();

    /**
     * @brief Get config file path
     * @return Full path to config file on SD card
     */
    static std::string get_config_path() { return "/sdcard/books/.opds_config.json"; }

  private:
    Config config;
    bool is_loaded;

    /**
     * @brief Ensure /books directory exists
     * @return true if directory exists or was created
     */
    bool ensure_books_dir();

    /**
     * @brief Parse JSON configuration string
     * @param json_str JSON configuration string
     * @return true if parsed successfully
     */
    bool parse_json(const std::string& json_str);

    /**
     * @brief Generate JSON from configuration
     * @return JSON string representation
     */
    std::string to_json() const;
};

#endif // M5_PAPER_S3
