#include "opds_client.hpp"
#include "esp_log.h"

static const char* TAG = "OPDSClient";

// Global singleton instance
OPDSClient g_opds_client;

OPDSClient::OPDSClient()
  : config(nullptr),
    http_client(nullptr),
    feed_parser(nullptr),
    download_manager(nullptr),
    is_initialized(false)
{
}

OPDSClient::~OPDSClient()
{
  cleanup();
}

bool OPDSClient::init()
{
  if (is_initialized) {
    ESP_LOGW(TAG, "Already initialized");
    return true;
  }

  // Create and initialize components
  config = std::make_unique<OPDSConfig>();
  http_client = std::make_unique<HTTPClient>();
  feed_parser = std::make_unique<OPDSFeedParser>();
  download_manager = std::make_unique<DownloadManager>(http_client.get());

  // Try to load existing configuration
  if (!config->load()) {
    ESP_LOGI(TAG, "No existing configuration, using defaults");
  }

  is_initialized = true;
  ESP_LOGI(TAG, "OPDS Client initialized");
  return true;
}

void OPDSClient::cleanup()
{
  download_manager.reset();
  feed_parser.reset();
  http_client.reset();
  config.reset();

  is_initialized = false;
  ESP_LOGI(TAG, "OPDS Client cleaned up");
}

bool OPDSClient::configure(const std::string& url,
                            const std::string& username,
                            const std::string& password,
                            bool use_https)
{
  if (!config) {
    ESP_LOGE(TAG, "Not initialized");
    return false;
  }

  OPDSConfig::Config cfg;
  cfg.url = url;
  cfg.username = username;
  cfg.password = password;
  cfg.use_https = use_https;

  config->set_config(cfg);

  if (config->save()) {
    ESP_LOGI(TAG, "Configuration saved: %s", url.c_str());
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to save configuration");
    return false;
  }
}

bool OPDSClient::fetch_catalog()
{
  if (!config || !http_client || !feed_parser) {
    ESP_LOGE(TAG, "Not initialized");
    return false;
  }

  const OPDSConfig::Config& cfg = config->get_config();
  std::string url = cfg.url;

  ESP_LOGI(TAG, "Fetching catalog from: %s", url.c_str());

  std::vector<uint8_t> response;
  if (!http_client->get_content(url, cfg.username, cfg.password, response)) {
    ESP_LOGE(TAG, "Failed to fetch catalog: %s", http_client->get_last_error().c_str());
    return false;
  }

  if (!feed_parser->parse(response)) {
    ESP_LOGE(TAG, "Failed to parse catalog feed");
    return false;
  }

  current_feed_url = url;
  ESP_LOGI(TAG, "Catalog loaded with %zu books", feed_parser->get_entry_count());
  return true;
}

bool OPDSClient::fetch_feed(const std::string& feed_url)
{
  if (!config || !http_client || !feed_parser) {
    ESP_LOGE(TAG, "Not initialized");
    return false;
  }

  if (feed_url.empty()) {
    ESP_LOGE(TAG, "Feed URL is empty");
    return false;
  }

  const OPDSConfig::Config& cfg = config->get_config();

  // Handle relative URLs
  std::string full_url = feed_url;
  if (feed_url[0] == '/' && !cfg.url.empty()) {
    // Extract base URL
    size_t schema_end = cfg.url.find("://");
    if (schema_end != std::string::npos) {
      size_t host_end = cfg.url.find("/", schema_end + 3);
      if (host_end != std::string::npos) {
        full_url = cfg.url.substr(0, host_end) + feed_url;
      }
    }
  }

  ESP_LOGI(TAG, "Fetching feed from: %s", full_url.c_str());

  std::vector<uint8_t> response;
  if (!http_client->get_content(full_url, cfg.username, cfg.password, response)) {
    ESP_LOGE(TAG, "Failed to fetch feed: %s", http_client->get_last_error().c_str());
    return false;
  }

  if (!feed_parser->parse(response)) {
    ESP_LOGE(TAG, "Failed to parse feed");
    return false;
  }

  current_feed_url = full_url;
  ESP_LOGI(TAG, "Feed loaded with %zu books", feed_parser->get_entry_count());
  return true;
}

std::vector<OPDSEntry> OPDSClient::get_current_entries() const
{
  if (!feed_parser) {
    return std::vector<OPDSEntry>();
  }

  return feed_parser->get_entries();
}

bool OPDSClient::next_page()
{
  if (!feed_parser) {
    ESP_LOGE(TAG, "Not initialized");
    return false;
  }

  std::string next_url = feed_parser->get_next_page_url();
  if (next_url.empty()) {
    ESP_LOGW(TAG, "No next page available");
    return false;
  }

  return fetch_feed(next_url);
}

bool OPDSClient::prev_page()
{
  if (!feed_parser) {
    ESP_LOGE(TAG, "Not initialized");
    return false;
  }

  std::string prev_url = feed_parser->get_prev_page_url();
  if (prev_url.empty()) {
    ESP_LOGW(TAG, "No previous page available");
    return false;
  }

  return fetch_feed(prev_url);
}

bool OPDSClient::has_next_page() const
{
  if (!feed_parser) {
    return false;
  }

  return feed_parser->has_next();
}

bool OPDSClient::has_prev_page() const
{
  if (!feed_parser) {
    return false;
  }

  return feed_parser->has_prev();
}

bool OPDSClient::search_books(const std::string& query)
{
  if (!config || !http_client || !feed_parser) {
    ESP_LOGE(TAG, "Not initialized");
    return false;
  }

  const OPDSConfig::Config& cfg = config->get_config();

  // Construct search URL
  // Assuming standard OPDS search URL format
  std::string search_url = cfg.url;
  if (search_url.back() != '/') {
    search_url += '/';
  }
  search_url += "search?query=" + url_encode(query);

  ESP_LOGI(TAG, "Searching for: %s", query.c_str());

  std::vector<uint8_t> response;
  if (!http_client->get_content(search_url, cfg.username, cfg.password, response)) {
    ESP_LOGE(TAG, "Search failed: %s", http_client->get_last_error().c_str());
    return false;
  }

  if (!feed_parser->parse(response)) {
    ESP_LOGE(TAG, "Failed to parse search results");
    return false;
  }

  current_feed_url = search_url;
  ESP_LOGI(TAG, "Search found %zu results", feed_parser->get_entry_count());
  return true;
}

bool OPDSClient::download_book(size_t entry_index)
{
  if (!download_manager) {
    ESP_LOGE(TAG, "Not initialized");
    return false;
  }

  if (!feed_parser) {
    ESP_LOGE(TAG, "No feed loaded");
    return false;
  }

  OPDSEntry entry = feed_parser->get_entry(entry_index);
  if (entry.title.empty() || entry.epub_url.empty()) {
    ESP_LOGE(TAG, "Invalid entry: %zu", entry_index);
    return false;
  }

  const OPDSConfig::Config& cfg = config->get_config();

  // Construct full EPUB URL if relative
  std::string full_epub_url = entry.epub_url;
  if (entry.epub_url[0] == '/' && !cfg.url.empty()) {
    size_t schema_end = cfg.url.find("://");
    if (schema_end != std::string::npos) {
      size_t host_end = cfg.url.find("/", schema_end + 3);
      if (host_end != std::string::npos) {
        full_epub_url = cfg.url.substr(0, host_end) + entry.epub_url;
      }
    }
  }

  // Construct file path
  std::string filename = entry.title;
  // Remove invalid filename characters
  for (size_t i = 0; i < filename.size(); i++) {
    char c = filename[i];
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      filename[i] = '_';
    }
  }
  filename += ".epub";

  std::string filepath = "/sdcard/books/" + filename;

  ESP_LOGI(TAG, "Starting download: %s -> %s", entry.title.c_str(), filepath.c_str());

  return download_manager->start_download(full_epub_url, cfg.username, cfg.password, filepath);
}

void OPDSClient::cancel_download()
{
  if (download_manager) {
    download_manager->cancel_download();
  }
}

bool OPDSClient::is_download_in_progress() const
{
  if (!download_manager) {
    return false;
  }

  return download_manager->is_downloading_now();
}

bool OPDSClient::is_download_complete() const
{
  if (!download_manager) {
    return false;
  }

  return download_manager->is_download_complete();
}

bool OPDSClient::was_download_successful() const
{
  if (!download_manager) {
    return false;
  }

  return download_manager->was_download_successful();
}

DownloadProgress OPDSClient::get_download_progress() const
{
  if (!download_manager) {
    return DownloadProgress();
  }

  return download_manager->get_progress();
}

std::string OPDSClient::url_encode(const std::string& str)
{
  std::string result;
  for (char c : str) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      result += c;
    } else if (c == ' ') {
      result += "%20";
    } else {
      result += "%" + to_hex(c);
    }
  }
  return result;
}

std::string OPDSClient::to_hex(uint8_t c)
{
  const char* hex_chars = "0123456789ABCDEF";
  std::string result;
  result += hex_chars[(c >> 4) & 0x0F];
  result += hex_chars[c & 0x0F];
  return result;
}

#endif // OPDS_CLIENT_CPP
