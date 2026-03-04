#include "http_client.hpp"
#include "esp_log.h"
#include "esp_http_client.h"
#include <cstring>
#include <algorithm>

static const char* TAG = "HTTPClient";

// Buffer structure for accumulating response data
struct http_buffer {
  std::vector<uint8_t> data;
  size_t total_size;
};

// Progress callback context
struct progress_ctx {
  HTTPClient::ProgressCallback progress_cb;
  HTTPClient::CancelCallback cancel_cb;
  uint64_t total_size;
  uint64_t downloaded;
};

// Static callback for esp_http_client
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
  http_buffer* buf = (http_buffer*)evt->user_data;
  
  switch(evt->event_id) {
    case HTTP_EVENT_ON_HEADER:
      // Get content length if available
      if (strcmp(evt->header_key, "Content-Length") == 0) {
        buf->total_size = atoi(evt->header_value);
      }
      break;

    case HTTP_EVENT_ON_DATA:
      if (!esp_http_client_is_chunked_response(evt->client)) {
        // Append data to buffer
        if (evt->data_len > 0) {
          size_t old_size = buf->data.size();
          buf->data.resize(old_size + evt->data_len);
          memcpy(buf->data.data() + old_size, evt->data, evt->data_len);
        }
      }
      break;

    case HTTP_EVENT_ERROR:
      ESP_LOGE(TAG, "HTTP Client Error");
      break;

    case HTTP_EVENT_ON_FINISH:
      // Finished
      break;

    default:
      break;
  }
  return ESP_OK;
}

HTTPClient::HTTPClient() : last_response_code(0)
{
}

HTTPClient::~HTTPClient()
{
}

bool HTTPClient::get_content(const std::string& url,
                              std::string& output_buffer,
                              const std::string& username,
                              const std::string& password)
{
  last_error.clear();
  output_buffer.clear();

  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.event_handler = &http_event_handler;

  http_buffer buffer = {};
  config.user_data = &buffer;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    last_error = "Failed to initialize HTTP client";
    ESP_LOGE(TAG, "%s", last_error.c_str());
    return false;
  }

  // Add basic authentication if provided
  if (!username.empty() && !password.empty()) {
    std::string auth = username + ":" + password;
    esp_http_client_set_authtype(client, HTTP_AUTH_TYPE_BASIC);
    esp_http_client_set_username(client, username.c_str());
    esp_http_client_set_password(client, password.c_str());
  }

  // Set User-Agent
  esp_http_client_set_header(client, "User-Agent", "EPub-InkPlate/1.0");

  esp_err_t ret = esp_http_client_perform(client);
  last_response_code = esp_http_client_get_status_code(client);

  if (ret != ESP_OK) {
    last_error = "HTTP request failed: " + std::string(esp_err_to_name(ret));
    ESP_LOGE(TAG, "%s", last_error.c_str());
    esp_http_client_cleanup(client);
    return false;
  }

  if (last_response_code != 200) {
    last_error = "HTTP " + std::to_string(last_response_code);
    ESP_LOGW(TAG, "HTTP response code: %d", last_response_code);
  }

  // Copy buffer to output
  output_buffer = std::string((const char*)buffer.data.data(), buffer.data.size());

  esp_http_client_cleanup(client);
  ESP_LOGI(TAG, "Downloaded %d bytes from %s", output_buffer.size(), url.c_str());
  
  return (last_response_code == 200);
}

bool HTTPClient::download_file(const std::string& url,
                                const std::string& username,
                                const std::string& password,
                                const std::string& filepath,
                                ProgressCallback progress_cb,
                                CancelCallback cancel_cb)
{
  last_error.clear();

  esp_http_client_config_t config = {};
  config.url = url.c_str();

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    last_error = "Failed to initialize HTTP client";
    ESP_LOGE(TAG, "%s", last_error.c_str());
    return false;
  }

  // Add basic authentication if provided
  if (!username.empty() && !password.empty()) {
    esp_http_client_set_authtype(client, HTTP_AUTH_TYPE_BASIC);
    esp_http_client_set_username(client, username.c_str());
    esp_http_client_set_password(client, password.c_str());
  }

  // Set User-Agent
  esp_http_client_set_header(client, "User-Agent", "EPub-InkPlate/1.0");

  esp_err_t ret = esp_http_client_open(client, 0);
  if (ret != ESP_OK) {
    last_error = "Failed to open HTTP connection: " + std::string(esp_err_to_name(ret));
    ESP_LOGE(TAG, "%s", last_error.c_str());
    esp_http_client_cleanup(client);
    return false;
  }

  // Get content length
  int content_len = esp_http_client_fetch_headers(client);
  if (content_len < 0) {
    last_error = "Failed to fetch headers";
    ESP_LOGE(TAG, "%s", last_error.c_str());
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
  }

  // Open file for writing
  FILE* f = fopen(filepath.c_str(), "wb");
  if (!f) {
    last_error = "Failed to open file for writing: " + filepath;
    ESP_LOGE(TAG, "%s", last_error.c_str());
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
  }

  // Download loop
  uint64_t downloaded = 0;
  uint64_t total = (content_len > 0) ? content_len : 0;
  const int CHUNK_SIZE = 4096;
  uint8_t buffer[CHUNK_SIZE];

  while (true) {
    // Check for cancel request
    if (cancel_cb && cancel_cb()) {
      ESP_LOGI(TAG, "Download cancelled by user");
      fclose(f);
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
      remove(filepath.c_str());  // Delete partial file
      last_error = "Download cancelled";
      return false;
    }

    int read_len = esp_http_client_read(client, (char*)buffer, CHUNK_SIZE);
    if (read_len < 0) {
      last_error = "Error during download";
      ESP_LOGE(TAG, "%s", last_error.c_str());
      fclose(f);
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
      remove(filepath.c_str());  // Delete partial file
      return false;
    }

    if (read_len == 0) {
      break;  // Download complete
    }

    // Write to file
    size_t written = fwrite(buffer, 1, read_len, f);
    if (written != read_len) {
      last_error = "Error writing to file";
      ESP_LOGE(TAG, "%s", last_error.c_str());
      fclose(f);
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
      remove(filepath.c_str());  // Delete partial file
      return false;
    }

    downloaded += read_len;

    // Report progress
    if (progress_cb) {
      progress_cb(downloaded, total);
    }
  }

  fclose(f);
  esp_http_client_close(client);
  last_response_code = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  ESP_LOGI(TAG, "Download complete: %llu bytes to %s", downloaded, filepath.c_str());
  return (last_response_code == 200);
}