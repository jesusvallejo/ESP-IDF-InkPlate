#include "opds_ui_manager.hpp"
#include "esp_log.h"
#include "driver/gpio.h"
#include <cmath>
#include <algorithm>

static const char* TAG = "OPDSUIManager";

// Global singleton
OPDSUIManager* g_opds_ui_manager = nullptr;

// UI Constants
const int SCREEN_WIDTH = 960;
const int SCREEN_HEIGHT = 540;
const int PADDING = 16;
const int LINE_HEIGHT = 24;
const int HEADER_HEIGHT = 40;
const int FOOTER_HEIGHT = 32;
const int ITEMS_PER_PAGE = 8;

OPDSUIManager::OPDSUIManager()
  : current_state(OPDS_STATE_MENU),
    previous_state(OPDS_STATE_MENU),
    panel(nullptr),
    opds_client(&g_opds_client),
    current_page_index(0),
    selected_book_index(0),
    display_offset(0),
    config_use_https(true),
    config_field_index(0)
{
  last_progress.percentage = 0;
  load_default_config();
}

OPDSUIManager::~OPDSUIManager()
{
}

bool OPDSUIManager::init(M5EPaperPanel* panel_ptr)
{
  if (!panel_ptr) {
    ESP_LOGE(TAG, "Panel pointer is null");
    return false;
  }

  panel = panel_ptr;

  // Initialize OPDS client
  if (!opds_client->init()) {
    ESP_LOGE(TAG, "Failed to initialize OPDS client");
    return false;
  }

  g_opds_ui_manager = this;
  ESP_LOGI(TAG, "OPDS UI Manager initialized");
  return true;
}

void OPDSUIManager::render()
{
  switch (current_state) {
    case OPDS_STATE_MENU:
      render_main_menu();
      break;
    case OPDS_STATE_CONFIG:
      render_config_menu();
      break;
    case OPDS_STATE_BROWSING:
      render_book_list();
      break;
    case OPDS_STATE_BOOK_DETAILS:
      render_book_details();
      break;
    case OPDS_STATE_DOWNLOADING:
      render_download_progress();
      break;
    case OPDS_STATE_DOWNLOAD_COMPLETE:
      render_download_complete();
      break;
    case OPDS_STATE_ERROR:
      render_error("An error occurred");
      break;
    case OPDS_STATE_CONNECT_FAILED:
      render_error("Failed to connect to OPDS server");
      break;
    default:
      break;
  }
}

void OPDSUIManager::render_main_menu()
{
  if (!panel) return;

  render_header("OPDS Book Catalog");

  int y = HEADER_HEIGHT + PADDING * 2;

  // Menu options
  std::vector<std::string> options = {
    "1. Configure Server",
    "2. Browse Books",
    "3. Download Recent",
    "4. Search Books",
    "5. Return to Main"
  };

  for (size_t i = 0; i < options.size(); i++) {
    // Draw option (simplified - would use panel->draw_string in actual implementation)
    y += LINE_HEIGHT + 8;
  }

  render_footer("Press corresponding number key");
}

void OPDSUIManager::render_config_menu()
{
  if (!panel) return;

  render_header("Configure OPDS Server");

  int y = HEADER_HEIGHT + PADDING * 2;

  // Configuration fields
  std::vector<std::string> fields = {
    "URL: " + config_url,
    "User: " + config_username,
    "Pass: " + (config_password.empty() ? "(not set)" : "****"),
    "HTTPS: " + std::string(config_use_https ? "Yes" : "No")
  };

  for (size_t i = 0; i < fields.size(); i++) {
    // Highlight current field
    if (i == config_field_index) {
      // Draw highlighted
    }
    y += LINE_HEIGHT + 8;
  }

  render_footer("Arrow to move, Enter to edit, * to save, 0 to cancel");
}

void OPDSUIManager::render_book_list()
{
  if (!panel || current_entries.empty()) {
    render_error("No books available");
    return;
  }

  render_header("OPDS Catalog");

  int y = HEADER_HEIGHT + PADDING;

  // Display books
  int books_to_show = std::min(ITEMS_PER_PAGE, (int)current_entries.size());
  for (int i = 0; i < books_to_show; i++) {
    size_t book_index = display_offset + i;
    if (book_index < current_entries.size()) {
      render_book_item(current_entries[book_index], y, book_index == selected_book_index);
      y += LINE_HEIGHT + 4;
    }
  }

  // Pagination info
  int current_page = (display_offset / ITEMS_PER_PAGE) + 1;
  int total_pages = (current_entries.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

  std::string pagination = "Page " + std::to_string(current_page) + "/" + std::to_string(total_pages);
  render_footer(pagination + " | Select: Enter | Up/Down: Nav | Back: 0");
}

void OPDSUIManager::render_book_details()
{
  if (!panel || selected_book_index >= current_entries.size()) {
    set_state(OPDS_STATE_BROWSING);
    return;
  }

  const OPDSEntry& entry = current_entries[selected_book_index];

  render_header(entry.title);

  int y = HEADER_HEIGHT + PADDING;

  // Cover placeholder
  // In full implementation, would render entry.cover_url image
  std::string cover_placeholder = "[Cover Image]";
  y += 100;  // Space for cover

  // Book details
  std::vector<std::string> details = {
    "Author: " + entry.author,
    "Size: " + format_size(entry.file_size),
    "Date: " + entry.publication_date,
    "ID: " + entry.id
  };

  for (const auto& detail : details) {
    y += LINE_HEIGHT + 4;
  }

  // Summary (truncated)
  if (!entry.summary.empty()) {
    std::string summary = entry.summary;
    if (summary.length() > 60) {
      summary = summary.substr(0, 60) + "...";
    }
    y += LINE_HEIGHT + 4;
  }

  render_footer("Download: Enter | Back: 0");
}

void OPDSUIManager::render_download_progress()
{
  if (!panel) return;

  render_header("Downloading...");

  const OPDSEntry& entry = current_entries[selected_book_index];
  int y = HEADER_HEIGHT + PADDING * 2;

  // Book title
  y += LINE_HEIGHT + 8;

  // Cover placeholder
  y += 100;

  // Progress information
  DownloadProgress progress = opds_client->get_download_progress();

  std::string downloaded_str = format_size(progress.downloaded_bytes);
  std::string total_str = format_size(progress.total_bytes);
  std::string speed_str = "Speed: " + std::to_string((int)progress.speed_mbps) + " MB/s";

  // Draw progress bar
  render_progress_bar(PADDING, y, SCREEN_WIDTH - PADDING * 2, 16, progress.percentage);
  y += 24;

  // Progress text
  std::string progress_text = std::to_string(progress.percentage) + "% (" + 
                             downloaded_str + " / " + total_str + ")";
  y += LINE_HEIGHT + 4;

  // Speed and ETA
  std::string eta_text = "ETA: " + format_duration(progress.estimated_remaining_seconds);
  y += LINE_HEIGHT + 4;

  render_footer("Cancel: 0");
}

void OPDSUIManager::render_download_complete()
{
  if (!panel) return;

  render_header("Download Complete");

  int y = HEADER_HEIGHT + PADDING * 4;

  std::string message = "Book successfully downloaded!";
  std::string location = "Location: /sdcard/books/";

  y += LINE_HEIGHT * 3;
  y += LINE_HEIGHT + 8;

  render_footer("Press Enter to continue");
}

void OPDSUIManager::render_error(const std::string& error_msg)
{
  if (!panel) return;

  render_header("Error");

  int y = HEADER_HEIGHT + PADDING * 4;

  // Error message (word-wrapped)
  // Simplified - would do proper text wrapping
  y += LINE_HEIGHT * 3;

  render_footer("Press Enter to continue");
}

void OPDSUIManager::on_button_pressed(int button_id)
{
  switch (current_state) {
    case OPDS_STATE_MENU:
      handle_menu_button(button_id);
      break;
    case OPDS_STATE_CONFIG:
      handle_config_button(button_id);
      break;
    case OPDS_STATE_BROWSING:
      handle_browse_button(button_id);
      break;
    case OPDS_STATE_BOOK_DETAILS:
      handle_details_button(button_id);
      break;
    case OPDS_STATE_DOWNLOADING:
      handle_download_button(button_id);
      break;
    default:
      break;
  }
}

void OPDSUIManager::handle_menu_button(int button_id)
{
  switch (button_id) {
    case 1:  // Configure
      set_state(OPDS_STATE_CONFIG);
      break;
    case 2:  // Browse
      fetch_catalog();
      break;
    case 3:  // Download recent
      // TODO: Implement recent downloads
      break;
    case 4:  // Search
      // TODO: Implement search
      break;
    case 0:  // Back/Exit
      // Return to main app
      break;
  }
}

void OPDSUIManager::handle_config_button(int button_id)
{
  switch (button_id) {
    case 8:  // Up arrow / Field up
      config_field_index = (config_field_index - 1 + 4) % 4;
      break;
    case 2:  // Down arrow / Field down
      config_field_index = (config_field_index + 1) % 4;
      break;
    case 10: // Enter / Edit field
      // TODO: Open text input for field
      break;
    case 42: // '*' to save
      save_configuration();
      set_state(OPDS_STATE_MENU);
      break;
    case 0:  // '0' to cancel
      set_state(OPDS_STATE_MENU);
      break;
  }
}

void OPDSUIManager::handle_browse_button(int button_id)
{
  switch (button_id) {
    case 8:  // Up arrow
      if (selected_book_index > 0) {
        selected_book_index--;
        if (selected_book_index < display_offset) {
          display_offset = selected_book_index;
        }
      }
      break;
    case 2:  // Down arrow
      if (selected_book_index < current_entries.size() - 1) {
        selected_book_index++;
        if (selected_book_index >= display_offset + ITEMS_PER_PAGE) {
          display_offset = selected_book_index - ITEMS_PER_PAGE + 1;
        }
      }
      break;
    case 4:  // Left arrow / Previous page
      if (opds_client->has_prev_page()) {
        opds_client->prev_page();
        fetch_catalog();
      }
      break;
    case 6:  // Right arrow / Next page
      if (opds_client->has_next_page()) {
        opds_client->next_page();
        fetch_catalog();
      }
      break;
    case 10: // Enter / Select book
      select_current_book();
      break;
    case 0:  // '0' to back
      set_state(OPDS_STATE_MENU);
      break;
  }
}

void OPDSUIManager::handle_details_button(int button_id)
{
  switch (button_id) {
    case 10: // Enter / Download
      start_book_download();
      break;
    case 0:  // '0' to back
      set_state(OPDS_STATE_BROWSING);
      break;
  }
}

void OPDSUIManager::handle_download_button(int button_id)
{
  switch (button_id) {
    case 0:  // '0' to cancel
      opds_client->cancel_download();
      set_state(OPDS_STATE_BOOK_DETAILS);
      break;
  }
}

void OPDSUIManager::update()
{
  if (current_state == OPDS_STATE_DOWNLOADING) {
    update_download_progress();

    if (opds_client->is_download_complete()) {
      if (opds_client->was_download_successful()) {
        set_state(OPDS_STATE_DOWNLOAD_COMPLETE);
      } else {
        show_error("Download failed: " + opds_client->get_download_progress().error_msg);
      }
    }
  }
}

void OPDSUIManager::update_download_progress()
{
  last_progress = opds_client->get_download_progress();
  render();  // Redraw with updated progress
}

void OPDSUIManager::set_state(OPDSUIState new_state)
{
  if (new_state == current_state) {
    return;
  }

  previous_state = current_state;
  current_state = new_state;

  // Notify callbacks
  for (auto& callback : state_change_callbacks) {
    callback(previous_state, new_state);
  }

  // Redraw
  render();
}

OPDSUIState OPDSUIManager::get_state() const
{
  return current_state;
}

void OPDSUIManager::on_state_change(StateChangeCallback callback)
{
  state_change_callbacks.push_back(callback);
}

void OPDSUIManager::on_error(ErrorCallback callback)
{
  error_callbacks.push_back(callback);
}

void OPDSUIManager::fetch_catalog()
{
  set_state(OPDS_STATE_BROWSING);

  if (!opds_client->fetch_catalog()) {
    show_error("Failed to fetch catalog: " + opds_client->get_http_client()->get_last_error());
    set_state(OPDS_STATE_CONNECT_FAILED);
    return;
  }

  current_entries = opds_client->get_current_entries();
  display_offset = 0;
  selected_book_index = 0;
  render();
}

void OPDSUIManager::select_current_book()
{
  if (selected_book_index >= current_entries.size()) {
    return;
  }

  set_state(OPDS_STATE_BOOK_DETAILS);
}

void OPDSUIManager::start_book_download()
{
  if (selected_book_index >= current_entries.size()) {
    return;
  }

  if (!opds_client->download_book(selected_book_index)) {
    show_error("Failed to start download");
    return;
  }

  set_state(OPDS_STATE_DOWNLOADING);
}

void OPDSUIManager::complete_download()
{
  set_state(OPDS_STATE_DOWNLOAD_COMPLETE);
}

void OPDSUIManager::show_error(const std::string& msg)
{
  ESP_LOGE(TAG, "%s", msg.c_str());

  for (auto& callback : error_callbacks) {
    callback(msg);
  }
}

void OPDSUIManager::render_header(const std::string& title)
{
  // Draw header background and title
  // Simplified - would use panel drawing functions
}

void OPDSUIManager::render_footer(const std::string& hint)
{
  // Draw footer background and hint text
  // Simplified - would use panel drawing functions
}

void OPDSUIManager::render_book_item(const OPDSEntry& entry, int y_pos, bool selected)
{
  // Draw book item with selection highlight
  std::string title = entry.title;
  if (title.length() > 60) {
    title = title.substr(0, 57) + "...";
  }

  // In full implementation, would draw icon, title, author
}

void OPDSUIManager::render_progress_bar(int x, int y, int width, int height, uint8_t percentage)
{
  // Draw progress bar outline and filled portion based on percentage
}

void OPDSUIManager::load_default_config()
{
  const OPDSConfig::Config& cfg = opds_client->get_config()->get_config();
  config_url = cfg.url.empty() ? "https://booklore.com/opds" : cfg.url;
  config_username = cfg.username;
  config_password = cfg.password;
  config_use_https = cfg.use_https;
}

void OPDSUIManager::save_configuration()
{
  opds_client->configure(config_url, config_username, config_password, config_use_https);
  ESP_LOGI(TAG, "Configuration saved");
}

std::string OPDSUIManager::format_size(uint64_t bytes)
{
  char buf[32];
  if (bytes < 1024) {
    snprintf(buf, sizeof(buf), "%llu B", bytes);
  } else if (bytes < 1024 * 1024) {
    snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0f);
  } else if (bytes < 1024 * 1024 * 1024) {
    snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0f * 1024.0f));
  } else {
    snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0f * 1024.0f * 1024.0f));
  }
  return std::string(buf);
}

std::string OPDSUIManager::format_duration(uint32_t seconds)
{
  char buf[32];
  uint32_t hours = seconds / 3600;
  uint32_t minutes = (seconds % 3600) / 60;
  uint32_t secs = seconds % 60;

  if (hours > 0) {
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hours, minutes, secs);
  } else if (minutes > 0) {
    snprintf(buf, sizeof(buf), "%02u:%02u", minutes, secs);
  } else {
    snprintf(buf, sizeof(buf), "%us", secs);
  }

  return std::string(buf);
}

#endif // OPDS_UI_MANAGER_CPP
