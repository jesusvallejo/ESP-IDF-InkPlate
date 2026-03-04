#pragma once

#include <string>
#include <functional>
#include <cstdint>

/**
 * @brief HTTP/HTTPS Client for OPDS operations
 * 
 * Simple wrapper around ESP-IDF HTTP client for downloading OPDS feeds and books.
 */
class HTTPClient
{
  public:
    /**
     * @brief Progress callback: (downloaded_bytes, total_bytes)
     */
    typedef std::function<void(uint64_t, uint64_t)> ProgressCallback;

    /**
     * @brief Cancellation callback: should return true to cancel
     */
    typedef std::function<bool()> CancelCallback;

    HTTPClient();
    ~HTTPClient();

    /**
     * @brief Fetch content from URL (for OPDS feeds)
     * @param url URL to fetch
     * @param username Optional username for basic auth
     * @param password Optional password for basic auth
     * @param output_buffer Buffer to store response
     * @return true if successful
     */
    bool get_content(const std::string& url, 
                     const std::string& username = "",
                     const std::string& password = "",
                     std::string& output_buffer = "");

    /**
     * @brief Download file with progress tracking
     * @param url URL to download from
     * @param dest_path Destination file path
     * @param username Optional username for basic auth
     * @param password Optional password for basic auth
     * @param progress_cb Optional progress callback
     * @param cancel_cb Optional cancellation callback
     * @return true if successful
     */
    bool download_file(const std::string& url,
                       const std::string& dest_path,
                       const std::string& username = "",
                       const std::string& password = "",
                       ProgressCallback progress_cb = nullptr,
                       CancelCallback cancel_cb = nullptr);

    /**
     * @brief Get last error message
     * @return Error description
     */
    std::string get_last_error() const { return last_error; }

    /**
     * @brief Get HTTP response code from last operation
     * @return HTTP status code (0 if error)
     */
    int get_response_code() const { return response_code; }

  private:
    std::string last_error;
    int response_code;

    /**
     * @brief Build basic auth header
     * @param username Username
     * @param password Password
     * @return Authorization header value
     */
    std::string build_auth_header(const std::string& username, const std::string& password);
};

#endif // M5_PAPER_S3
