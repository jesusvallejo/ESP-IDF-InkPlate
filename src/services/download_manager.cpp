#include "download_manager.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ctime>
#include <cmath>

static const char* TAG = "DownloadManager";

// Global task handle
static TaskHandle_t download_task_handle = nullptr;

// Context for download task
struct DownloadTaskContext {
  HTTPClient* http_client;
  std::string url;
  std::string username;
  std::string password;
  std::string filepath;
  DownloadManager* manager;
};

// FreeRTOS task for async download
static void download_task(void* pvParameters)
{
  DownloadTaskContext* ctx = (DownloadTaskContext*)pvParameters;

  // Execute download
  bool success = ctx->http_client->download_file(
    ctx->url,
    ctx->username,
    ctx->password,
    ctx->filepath,
    [ctx](uint64_t downloaded, uint64_t total) {
      ctx->manager->update_progress(downloaded, total);
    },
    [ctx]() {
      return ctx->manager->is_cancel_requested();
    }
  );

  // Update final status
  ctx->manager->set_download_complete(success);

  // Cleanup
  delete ctx;
  download_task_handle = nullptr;

  vTaskDelete(nullptr);
}

DownloadManager::DownloadManager(HTTPClient* http_client)
  : http_client(http_client),
    is_downloading(false),
    cancel_requested(false),
    is_complete(false),
    download_success(false)
{
  progress.downloaded_bytes = 0;
  progress.total_bytes = 0;
  progress.percentage = 0;
  progress.speed_mbps = 0.0f;
  progress.epoch_start = 0;
  progress.epoch_last_update = 0;
  progress.last_downloaded = 0;
}

DownloadManager::~DownloadManager()
{
  if (is_downloading) {
    cancel_download();
  }
}

bool DownloadManager::start_download(const std::string& url,
                                      const std::string& username,
                                      const std::string& password,
                                      const std::string& filepath)
{
  if (is_downloading) {
    ESP_LOGW(TAG, "Download already in progress");
    return false;
  }

  // Reset state
  is_downloading = true;
  cancel_requested = false;
  is_complete = false;
  download_success = false;

  progress.downloaded_bytes = 0;
  progress.total_bytes = 0;
  progress.percentage = 0;
  progress.speed_mbps = 0.0f;
  progress.epoch_start = time(nullptr);
  progress.epoch_last_update = progress.epoch_start;
  progress.last_downloaded = 0;

  // Create task context
  DownloadTaskContext* ctx = new DownloadTaskContext();
  ctx->http_client = http_client;
  ctx->url = url;
  ctx->username = username;
  ctx->password = password;
  ctx->filepath = filepath;
  ctx->manager = this;

  // Create FreeRTOS task for async download
  BaseType_t ret = xTaskCreate(
    download_task,
    "DownloadTask",
    8192,  // Stack size
    ctx,
    5,     // Priority
    &download_task_handle
  );

  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create download task");
    delete ctx;
    is_downloading = false;
    return false;
  }

  ESP_LOGI(TAG, "Download started: %s", url.c_str());
  return true;
}

void DownloadManager::cancel_download()
{
  if (!is_downloading) {
    return;
  }

  ESP_LOGI(TAG, "Cancel requested");
  cancel_requested = true;

  // Wait for task to complete (max 5 seconds)
  int attempts = 0;
  while (is_downloading && attempts < 50) {
    vTaskDelay(pdMS_TO_TICKS(100));
    attempts++;
  }

  if (is_downloading) {
    ESP_LOGW(TAG, "Download task did not stop within timeout");
  }
}

void DownloadManager::update_progress(uint64_t downloaded, uint64_t total)
{
  progress.downloaded_bytes = downloaded;
  progress.total_bytes = total;

  if (total > 0) {
    progress.percentage = (downloaded * 100) / total;
  }

  // Calculate speed
  time_t now = time(nullptr);
  if (now > progress.epoch_last_update) {
    uint64_t bytes_since_update = downloaded - progress.last_downloaded;
    double time_elapsed = difftime(now, progress.epoch_last_update);

    if (time_elapsed > 0) {
      double speed_bps = bytes_since_update / time_elapsed;
      progress.speed_mbps = speed_bps / (1024.0f * 1024.0f);
    }

    progress.epoch_last_update = now;
    progress.last_downloaded = downloaded;

    // Calculate ETA
    if (progress.speed_mbps > 0 && total > downloaded) {
      double remaining_bytes = total - downloaded;
      double remaining_seconds = remaining_bytes / (progress.speed_mbps * 1024.0f * 1024.0f);
      progress.estimated_remaining_seconds = (uint32_t)remaining_seconds;
    } else {
      progress.estimated_remaining_seconds = 0;
    }

    // Calculate elapsed
    progress.elapsed_seconds = (uint32_t)difftime(now, progress.epoch_start);
  }
}

void DownloadManager::set_download_complete(bool success)
{
  is_downloading = false;
  is_complete = true;
  download_success = success;

  if (success) {
    ESP_LOGI(TAG, "Download completed successfully");
  } else {
    ESP_LOGE(TAG, "Download failed");
  }
}

bool DownloadManager::is_downloading_now() const
{
  return is_downloading;
}

bool DownloadManager::is_cancel_requested() const
{
  return cancel_requested;
}

bool DownloadManager::is_download_complete() const
{
  return is_complete;
}

bool DownloadManager::was_download_successful() const
{
  return download_success;
}

DownloadProgress DownloadManager::get_progress() const
{
  return progress;
}

float DownloadManager::get_speed_mbps() const
{
  return progress.speed_mbps;
}

uint8_t DownloadManager::get_percentage() const
{
  return progress.percentage;
}

uint32_t DownloadManager::get_elapsed_seconds() const
{
  return progress.elapsed_seconds;
}

uint32_t DownloadManager::get_estimated_remaining_seconds() const
{
  return progress.estimated_remaining_seconds;
}

std::string DownloadManager::format_size(uint64_t bytes)
{
  char buf[32];
  if (bytes < 1024) {
    snprintf(buf, sizeof(buf), "%llu B", bytes);
  } else if (bytes < 1024 * 1024) {
    snprintf(buf, sizeof(buf), "%.2f KB", bytes / 1024.0f);
  } else if (bytes < 1024 * 1024 * 1024) {
    snprintf(buf, sizeof(buf), "%.2f MB", bytes / (1024.0f * 1024.0f));
  } else {
    snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0f * 1024.0f * 1024.0f));
  }
  return std::string(buf);
}

std::string DownloadManager::format_time(uint32_t seconds)
{
  char buf[32];
  uint32_t hours = seconds / 3600;
  uint32_t minutes = (seconds % 3600) / 60;
  uint32_t secs = seconds % 60;

  if (hours > 0) {
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hours, minutes, secs);
  } else {
    snprintf(buf, sizeof(buf), "%02u:%02u", minutes, secs);
  }

  return std::string(buf);
}

#endif // DOWNLOAD_MANAGER_CPP
