#pragma once

#include <string>
#include <functional>
#include <cstdint>
#include <ctime>

/**
 * @brief Download Progress Information
 */
struct DownloadProgress
{
  uint64_t downloaded_bytes;         // Bytes downloaded so far
  uint64_t total_bytes;              // Total bytes to download (0 if unknown)
  uint8_t percentage;                // Progress percentage (0-100)
  float speed_mbps;                  // Current download speed in MB/s
  uint32_t elapsed_seconds;          // Time elapsed
  uint32_t estimated_remaining_seconds; // Estimated time remaining
  time_t epoch_start;                // Download start time
  time_t epoch_last_update;          // Last progress update time
  uint64_t last_downloaded;          // Bytes at last update (for speed calc)
  std::string error_msg;             // Error message if download failed
};

/**
 * @brief Download Manager
 * 
 * Manages book downloads with progress tracking and cancellation support.
 */
class DownloadManager
{
  public:
    /**
     * @brief Download progress callback
     */
    typedef std::function<void(const DownloadProgress&)> ProgressCallback;

    /**
     * @brief Cancellation callback: returns true to cancel
     */
    typedef std::function<bool()> CancelCallback;

    DownloadManager();
    ~DownloadManager();

    /**
     * @brief Start downloading a book
     * @param url Download URL
     * @param title Book title
     * @param author Book author
     * @param cover_url Cover image URL (optional)
     * @param username Optional username for auth
     * @param password Optional password for auth
     * @param progress_cb Progress callback
     * @param cancel_cb Cancellation callback
     * @return true if download started successfully
     */
    bool start_download(const std::string& url,
                       const std::string& title,
                       const std::string& author,
                       const std::string& cover_url = "",
                       const std::string& username = "",
                       const std::string& password = "",
                       ProgressCallback progress_cb = nullptr,
                       CancelCallback cancel_cb = nullptr);

    /**
     * @brief Check if download is in progress
     * @return true if currently downloading
     */
    bool is_downloading() const { return downloading; }

    /**
     * @brief Get current download progress
     * @return Current progress information
     */
    const DownloadProgress& get_progress() const { return progress; }

    /**
     * @brief Cancel current download
     * Removes partially downloaded file
     * @return true if cancelled successfully
     */
    bool cancel_download();

    /**
     * @brief Get current download file path
     * @return Path to book being downloaded
     */
    std::string get_current_file_path() const { return current_file_path; }

    /**
     * @brief Get last error message
     * @return Error description
     */
    std::string get_last_error() const { return last_error; }

    /**
     * @brief Check if download was completed successfully
     * @return true if last download completed
     */
    bool is_last_download_complete() const { return download_complete; }

    /**
     * @brief Get downloaded book path after completion
     * @return Full path to downloaded EPUB
     */
    std::string get_downloaded_file_path() const { return downloaded_file_path; }

    // Internal helper methods for progress tracking
    void update_progress(uint64_t downloaded, uint64_t total);
    void set_download_complete(bool success);
    bool is_downloading_now() const;
    bool is_cancel_requested() const;
    bool is_download_complete() const;
    bool was_download_successful() const;
    DownloadProgress get_progress_struct() const { return progress; }
    float get_speed_mbps() const;
    uint8_t get_percentage() const;
    uint32_t get_elapsed_seconds() const;
    uint32_t get_estimated_remaining_seconds() const;
    
    static std::string format_size(uint64_t bytes);
    static std::string format_time(uint32_t seconds);

  private:
    bool downloading;
    bool download_complete;
    bool cancel_requested;
    bool is_complete;
    bool download_success;
    DownloadProgress progress;
    std::string current_file_path;
    std::string downloaded_file_path;
    std::string last_error;
    std::string temp_file_path;

    /**
     * @brief Generate destination file path for book
     * @param title Book title
     * @return Full path where book should be saved
     */
    std::string generate_dest_path(const std::string& title);
};

// Global download manager singleton
extern DownloadManager* g_download_manager;
