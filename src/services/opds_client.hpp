#pragma once

#if M5_PAPER_S3

#include "opds_config.hpp"
#include "opds_feed_parser.hpp"
#include "http_client.hpp"
#include "download_manager.hpp"

#include <string>
#include <vector>
#include <memory>

/**
 * @brief OPDS Client
 * 
 * Complete OPDS client for browsing and downloading books from OPDS servers.
 * Integrates configuration, HTTP client, feed parsing, and download management.
 */
class OPDSClient
{
  public:
    OPDSClient();
    ~OPDSClient();

    /**
     * @brief Initialize OPDS client
     * Load configuration from SD card
     * @return true if successful
     */
    bool init();

    /**
     * @brief Check if configured (has server URL)
     * @return true if configured
     */
    bool is_configured() const { return config && !config->get_config().url.empty(); }

    /**
     * @brief Configure OPDS server
     * @param url Server URL (e.g., https://booklore.com/opds)
     * @param username Optional username
     * @param password Optional password
     * @param use_https Whether to use HTTPS
     * @return true if configuration saved
     */
    bool configure(const std::string& url,
                  const std::string& username = "",
                  const std::string& password = "",
                  bool use_https = true);

    /**
     * @brief Fetch and parse main catalog
     * @return true if successful
     */
    bool fetch_catalog();

    /**
     * @brief Fetch and parse specific feed
     * @param feed_url URL to specific OPDS feed
     * @return true if successful
     */
    bool fetch_feed(const std::string& feed_url);

    /**
     * @brief Search for books
     * @param query Search query
     * @return true if search successful
     */
    bool search_books(const std::string& query);

    /**
     * @brief Get current feed entries
     * @return Vector of book entries
     */
    const std::vector<OPDSEntry>& get_entries() const;

    /**
     * @brief Get current feed title
     * @return Feed title/category name
     */
    std::string get_current_feed_title() const;

    /**
     * @brief Download book
     * @param entry_index Index of book in current entries
     * @param progress_cb Progress callback
     * @param cancel_cb Cancellation callback
     * @return true if download started
     */
    bool download_book(size_t entry_index,
                      DownloadManager::ProgressCallback progress_cb = nullptr,
                      DownloadManager::CancelCallback cancel_cb = nullptr);

    /**
     * @brief Get download manager
     * @return Reference to download manager
     */
    DownloadManager& get_download_manager() { return download_mgr; }

    /**
     * @brief Get configuration object
     * @return Pointer to config object
     */
    OPDSConfig* get_config() { return config.get(); }

    /**
     * @brief Get HTTP client
     * @return Pointer to HTTP client
     */
    HTTPClient* get_http_client() { return http_client.get(); }

    /**
     * @brief Navigate to next page in current feed
     * @return true if successful
     */
    bool next_page();

    /**
     * @brief Navigate to previous page in current feed
     * @return true if successful
     */
    bool prev_page();

    /**
     * @brief Get last error message
     * @return Error description
     */
    std::string get_last_error() const { return last_error; }

    /**
     * @brief Check if network is available
     * @return true if WiFi is connected
     */
    bool is_network_available() const;

  private:
    std::unique_ptr<OPDSConfig> config;
    std::unique_ptr<HTTPClient> http_client;
    std::unique_ptr<OPDSFeedParser> feed_parser;
    std::unique_ptr<DownloadManager> download_manager;
    bool is_initialized;
    std::string last_error;
    std::string current_feed_url;

    /**
     * @brief Build OPDS server base URL
     * @return Full base URL for OPDS server
     */
    std::string build_server_url() const;

    /**
     * @brief Default OPDS endpoints
     */
    static constexpr const char* CATALOG_PATH = "/opds";
    static constexpr const char* SEARCH_PATH = "/opds/search";
};

// Global singleton instance
extern OPDSClient g_opds_client;

#endif // M5_PAPER_S3
