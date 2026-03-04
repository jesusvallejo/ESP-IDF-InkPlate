#include "opds_ui_manager.hpp"
#include "esp_log.h"
#include "driver/gpio.h"
#include <cmath>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <cstring>

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
    config_field_index(0),
    touch_in_progress(false),
    touch_regions_for_keyboard(0),
    keyboard_mode("")
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

  clear_touch_regions();
  render_header("OPDS Book Catalog");

  int y = HEADER_HEIGHT + PADDING * 4;
  int button_width = SCREEN_WIDTH - PADDING * 4;
  int button_height = 48;

  // Menu option buttons with touch regions
  std::vector<std::pair<std::string, std::string>> menu_items = {
    {"Configure Server", "config"},
    {"Browse Catalog", "browse"},
    {"Recent Downloads", "recent"},
    {"Search Books", "search"},
    {"Exit", "exit"}
  };

  for (const auto& item : menu_items) {
    draw_touch_button(PADDING * 2, y, button_width, button_height, item.first);
    add_touch_region(PADDING * 2, y, button_width, button_height, "menu_" + item.second);
    ESP_LOGD(TAG, "Menu button: %s", item.first.c_str());
    y += button_height + PADDING;
  }

  render_footer("Tap menu option to select");
}

void OPDSUIManager::render_config_menu()
{
  if (!panel) return;

  clear_touch_regions();
  render_header("Configure OPDS Server");

  int y = HEADER_HEIGHT + PADDING * 2;
  int field_height = 48;

  // Configuration fields as touch buttons
  std::vector<std::pair<std::string, int>> config_fields = {
    {"URL: " + (config_url.empty() ? "(not set)" : config_url.substr(0, 30)), 0},
    {"User: " + (config_username.empty() ? "(not set)" : config_username), 1},
    {"Pass: " + (config_password.empty() ? "(not set)" : std::string(config_password.length(), '*')), 2},
    {"HTTPS: " + std::string(config_use_https ? "Enabled" : "Disabled"), 3}
  };

  for (const auto& field : config_fields) {
    draw_touch_button(PADDING, y, SCREEN_WIDTH - PADDING * 2, field_height, field.first);
    add_touch_region(PADDING, y, SCREEN_WIDTH - PADDING * 2, field_height, "field_" + std::to_string(field.second));
    ESP_LOGD(TAG, "Config field %d: %s", field.second, field.first.c_str());
    y += field_height + PADDING;
  }

  // Save and Cancel buttons
  int button_width = (SCREEN_WIDTH - PADDING * 3) / 2;
  y += PADDING * 2;

  draw_touch_button(PADDING, y, button_width, 44, "Save");
  add_touch_region(PADDING, y, button_width, 44, "config_save");

  draw_touch_button(PADDING + button_width + PADDING, y, button_width, 44, "Cancel");
  add_touch_region(PADDING + button_width + PADDING, y, button_width, 44, "config_cancel");

  render_footer("Tap field to edit | Save to confirm");
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

  // Draw cover image or placeholder
  if (!entry.cover_url.empty()) {
    // Load and render cover image from URL
    ESP_LOGI(TAG, "Rendering cover image from: %s", entry.cover_url.c_str());
    // panel->load_image(entry.cover_url.c_str(), PADDING, y, 100, 100);
    panel->fill_rect(PADDING, y, 100, 100, 200);  // Light gray placeholder
    panel->draw_rect(PADDING, y, 100, 100, 0);    // Black border
    panel->draw_string(PADDING + 20, y + 40, "[Cover]", 12, 0);
  } else {
    ESP_LOGD(TAG, "No cover URL provided");
    panel->fill_rect(PADDING, y, 100, 100, 200);  // Light gray placeholder
    panel->draw_rect(PADDING, y, 100, 100, 0);    // Black border
    panel->draw_string(PADDING + 20, y + 40, "[No Img]", 12, 0);
  }
  y += 100;  // Space for cover

  // Book details
  std::vector<std::string> details = {
    "Author: " + entry.author,
    "Size: " + format_size(entry.file_size),
    "Date: " + entry.publication_date,
    "ID: " + entry.id
  };

  for (const auto& detail : details) {
    ESP_LOGD(TAG, "%s", detail.c_str());
    y += LINE_HEIGHT + 4;
  }

  // Summary (truncated)
  if (!entry.summary.empty()) {
    std::string summary = entry.summary;
    if (summary.length() > 60) {
      summary = summary.substr(0, 60) + "...";
    }
    ESP_LOGD(TAG, "Summary: %s", summary.c_str());
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

  // Draw cover image or placeholder
  if (!entry.cover_url.empty()) {
    ESP_LOGI(TAG, "Rendering download cover from: %s", entry.cover_url.c_str());
    panel->fill_rect(PADDING, y, 100, 100, 200);  // Light gray placeholder
    panel->draw_rect(PADDING, y, 100, 100, 0);    // Black border
    panel->draw_string(PADDING + 20, y + 40, "[Cover]", 12, 0);
  } else {
    panel->fill_rect(PADDING, y, 100, 100, 200);  // Light gray placeholder
    panel->draw_rect(PADDING, y, 100, 100, 0);
  }
  y += 100;  // Space for cover

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
  ESP_LOGD(TAG, "%s", progress_text.c_str());
  y += LINE_HEIGHT + 4;

  // Speed and ETA
  ESP_LOGD(TAG, "%s", speed_str.c_str());
  std::string eta_text = "ETA: " + format_duration(progress.estimated_remaining_seconds);
  ESP_LOGD(TAG, "%s", eta_text.c_str());
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

  ESP_LOGD(TAG, "%s", message.c_str());
  y += LINE_HEIGHT * 3;
  
  ESP_LOGD(TAG, "%s", location.c_str());
  y += LINE_HEIGHT + 8;

  render_footer("Press Enter to continue");
}

void OPDSUIManager::render_error(const std::string& error_msg)
{
  if (!panel) return;

  render_header("Error");

  int y = HEADER_HEIGHT + PADDING * 4;

  // Error message (word-wrapped)
  std::string msg = error_msg;
  while (!msg.empty()) {
    std::string line;
    if (msg.length() > 70) {
      line = msg.substr(0, 70);
      msg = msg.substr(70);
    } else {
      line = msg;
      msg.clear();
    }
    ESP_LOGE(TAG, "%s", line.c_str());
    y += LINE_HEIGHT + 4;
  }

  render_footer("Press Enter to continue");
}

void OPDSUIManager::on_touch_event(int x, int y, bool pressed)
{
  if (!pressed) {
    touch_in_progress = false;
    return;  // Only handle press events
  }

  touch_in_progress = true;
  ESP_LOGI(TAG, "Touch event: (%d, %d)", x, y);

  switch (current_state) {
    case OPDS_STATE_MENU:
      handle_menu_touch(x, y);
      break;
    case OPDS_STATE_CONFIG:
      handle_config_touch(x, y);
      break;
    case OPDS_STATE_BROWSING:
      handle_browse_touch(x, y);
      break;
    case OPDS_STATE_BOOK_DETAILS:
      handle_details_touch(x, y);
      break;
    case OPDS_STATE_DOWNLOADING:
      handle_download_touch(x, y);
      break;
    default:
      break;
  }
}

void OPDSUIManager::clear_touch_regions()
{
  touch_regions.clear();
}

void OPDSUIManager::add_touch_region(int x, int y, int width, int height, const std::string& action)
{
  touch_regions.push_back({x, y, width, height, action});
  ESP_LOGD(TAG, "Added touch region: (%d,%d) %dx%d action='%s'", x, y, width, height, action.c_str());
}

OPDSUIManager::TouchRegion* OPDSUIManager::get_touched_region(int x, int y)
{
  for (auto& region : touch_regions) {
    if (x >= region.x && x <= region.x + region.width &&
        y >= region.y && y <= region.y + region.height) {
      ESP_LOGD(TAG, "Touch hit region: %s", region.action.c_str());
      return &region;
    }
  }
  return nullptr;
}

void OPDSUIManager::draw_touch_button(int x, int y, int width, int height, const std::string& label, bool highlighted)
{
  if (!panel) return;

  // Draw button background
  if (highlighted) {
    panel->fill_rect(x, y, width, height, 128);  // Gray for highlighted
  } else {
    panel->fill_rect(x, y, width, height, 255);  // White background
  }

  // Draw button border
  panel->draw_rect(x, y, width, height, 0);  // Black border

  // Draw label text centered in button
  int text_x = x + PADDING;
  int text_y = y + (height - 16) / 2;  // Center vertically (16pt text)
  panel->draw_string(text_x, text_y, label.c_str(), 16, 0);

  ESP_LOGD(TAG, "Button drawn: '%s' at (%d,%d)", label.c_str(), x, y);
}

void OPDSUIManager::handle_menu_touch(int x, int y)
{
  TouchRegion* region = get_touched_region(x, y);
  if (!region) {
    ESP_LOGW(TAG, "Touch outside menu buttons: (%d,%d)", x, y);
    return;
  }

  ESP_LOGI(TAG, "Menu action: %s", region->action.c_str());

  if (region->action == "menu_config") {
    set_state(OPDS_STATE_CONFIG);
  } else if (region->action == "menu_browse") {
    fetch_catalog();
  } else if (region->action == "menu_recent") {
    show_recent_downloads();
  } else if (region->action == "menu_search") {
    show_search_ui();
  } else if (region->action == "menu_exit") {
    ESP_LOGI(TAG, "Exit OPDS menu");
  }
}

void OPDSUIManager::handle_config_touch(int x, int y)
{
  TouchRegion* region = get_touched_region(x, y);
  if (!region) {
    return;
  }

  ESP_LOGI(TAG, "Config action: %s", region->action.c_str());

  // Check if it's a field edit region (URLs, usernames, etc)
  if (region->action.substr(0, 6) == "field_") {
    std::string field_str = region->action.substr(6);
    int field_index = std::stoi(field_str);
    edit_config_field(field_index);
  }
  // Check for button regions
  else if (region->action == "config_save") {
    save_configuration();
    set_state(OPDS_STATE_MENU);
  } else if (region->action == "config_cancel") {
    set_state(OPDS_STATE_MENU);
  }
}

void OPDSUIManager::handle_browse_touch(int x, int y)
{
  TouchRegion* region = get_touched_region(x, y);
  if (!region) {
    return;
  }

  ESP_LOGI(TAG, "Browse action: %s", region->action.c_str());

  // Check if it's a book item touch
  if (region->action.substr(0, 5) == "book_") {
    std::string book_str = region->action.substr(5);
    int book_index = std::stoi(book_str);
    selected_book_index = display_offset + book_index;
    select_current_book();
  }
  // Check for navigation buttons
  else if (region->action == "browse_prev") {
    if (opds_client->has_prev_page()) {
      opds_client->prev_page();
      fetch_catalog();
    }
  } else if (region->action == "browse_next") {
    if (opds_client->has_next_page()) {
      opds_client->next_page();
      fetch_catalog();
    }
  } else if (region->action == "browse_back") {
    set_state(OPDS_STATE_MENU);
  }
}

void OPDSUIManager::handle_details_touch(int x, int y)
{
  TouchRegion* region = get_touched_region(x, y);
  if (!region) {
    return;
  }

  ESP_LOGI(TAG, "Details action: %s", region->action.c_str());

  if (region->action == "details_download") {
    start_book_download();
  } else if (region->action == "details_back") {
    set_state(OPDS_STATE_BROWSING);
  }
}

void OPDSUIManager::handle_download_touch(int x, int y)
{
  TouchRegion* region = get_touched_region(x, y);
  if (!region) {
    return;
  }

  ESP_LOGI(TAG, "Download action: %s", region->action.c_str());

  if (region->action == "download_cancel") {
    opds_client->cancel_download();
    set_state(OPDS_STATE_BOOK_DETAILS);
  }
}

void OPDSUIManager::draw_on_screen_keyboard(int x, int y)
{
  if (!panel) return;

  // Helper to draw full on-screen keyboard
  // Keyboard layout: QWERTY style
  const std::string rows[] = {
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM.",
    "0123456789"
  };

  int row_y = y;
  for (int row = 0; row < 4; row++) {
    int col_x = x;
    for (char c : rows[row]) {
      std::string key_str(1, c);
      render_keyboard_letter(col_x, row_y, c, col_x, row_y);
      col_x += 60;  // Key spacing
    }
    row_y += 32;  // Row spacing
  }

  ESP_LOGD(TAG, "On-screen keyboard rendered at (%d,%d)", x, y);
}

void OPDSUIManager::render_keyboard_letter(int col, int row, char letter, int x, int y, bool pressed)
{
  if (!panel) return;

  int key_width = 56;
  int key_height = 28;

  // Draw key background
  if (pressed) {
    panel->fill_rect(x, y, key_width, key_height, 0);      // Dark when pressed
  } else {
    panel->fill_rect(x, y, key_width, key_height, 200);    // Light gray
  }

  // Draw border
  panel->draw_rect(x, y, key_width, key_height, 0);

  // Draw character
  std::string key_str(1, letter);
  int text_color = pressed ? 255 : 0;  // Inverted if pressed
  panel->draw_string(x + 20, y + 6, key_str.c_str(), 12, text_color);

  if (pressed) {
    ESP_LOGD(TAG, "Keyboard key '%c' pressed", letter);
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
      // Load recently downloaded books from /sdcard/books/
      show_recent_downloads();
      break;
    case 4:  // Search
      // Open search UI
      show_search_ui();
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
      edit_config_field(config_field_index);
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
  if (!panel) return;

  // Draw header background and title
  // Fill header area with white background
  panel->fill_rect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, 0);  // White background
  
  // Draw title text in larger font (24pt)
  panel->draw_string(PADDING, PADDING/2, title.c_str(), 24, 0);  // Black text
  
  // Draw horizontal border line at bottom of header
  panel->draw_line(0, HEADER_HEIGHT, SCREEN_WIDTH, HEADER_HEIGHT, 0);  // Border
  
  ESP_LOGD(TAG, "Header rendered: '%s'", title.c_str());
}

void OPDSUIManager::render_footer(const std::string& hint)
{
  if (!panel) return;

  // Draw footer background and hint text at bottom
  int footer_y = SCREEN_HEIGHT - FOOTER_HEIGHT;
  
  // Fill footer area with white background
  panel->fill_rect(0, footer_y, SCREEN_WIDTH, FOOTER_HEIGHT, 0);  // White background
  
  // Draw hint text in smaller font (16pt) with padding
  panel->draw_string(PADDING, footer_y + 4, hint.c_str(), 16, 0);  // Smaller text
  
  // Draw horizontal border line at top of footer
  panel->draw_line(0, footer_y, SCREEN_WIDTH, footer_y, 0);  // Border
  
  ESP_LOGD(TAG, "Footer rendered at y=%d: '%s'", footer_y, hint.c_str());
}

void OPDSUIManager::render_book_item(const OPDSEntry& entry, int y_pos, bool selected)
{
  if (!panel) return;

  // Draw book item with selection highlight
  std::string title = entry.title;
  if (title.length() > 60) {
    title = title.substr(0, 57) + "...";
  }

  // Draw selection highlight background if selected
  if (selected) {
    // Gray background (gray value 128) for selected item
    panel->fill_rect(PADDING, y_pos, SCREEN_WIDTH - PADDING*2, LINE_HEIGHT, 128);  // Gray highlight
    ESP_LOGD(TAG, "Selected item highlighted at y=%d", y_pos);
  }
  
  // Draw book title in medium font (18pt)
  panel->draw_string(PADDING * 2, y_pos, title.c_str(), 18, 0);
  
  // Draw author name in smaller font (12pt), one line below title
  panel->draw_string(PADDING * 2, y_pos + 16, entry.author.c_str(), 12, 0);
  
  // Draw file size on right side if available
  if (entry.file_size > 0) {
    std::string size_str = DownloadManager::format_size(entry.file_size);
    panel->draw_string(SCREEN_WIDTH - PADDING * 4, y_pos, size_str.c_str(), 12, 0);
    ESP_LOGD(TAG, "Item size: %s", size_str.c_str());
  }

  ESP_LOGD(TAG, "Item [%s]: %s by %s", selected ? "*" : " ", 
           title.c_str(), entry.author.c_str());
}

void OPDSUIManager::render_progress_bar(int x, int y, int width, int height, uint8_t percentage)
{
  if (!panel) return;

  // Draw progress bar outline and filled portion based on percentage
  // Draw border rectangle
  panel->draw_rect(x, y, width, height, 0);
  
  // Calculate fill width based on percentage
  int fill_width = (width - 2) * percentage / 100;
  
  // Draw filled portion (black fill)
  if (fill_width > 0) {
    panel->fill_rect(x + 1, y + 1, fill_width, height - 2, 0);
    ESP_LOGD(TAG, "Progress bar fill: %d%% (%d/%d pixels)", percentage, fill_width, width - 2);
  }
  
  // Log visual representation
  std::string bar_visual = std::string(percentage / 5, '=') + std::string(20 - percentage / 5, '-');
  ESP_LOGD(TAG, "Progress bar: %d%% [%s]", percentage, bar_visual.c_str());
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

void OPDSUIManager::show_recent_downloads()
{
  if (!panel) {
    ESP_LOGE(TAG, "Panel not initialized");
    return;
  }

  // Load recently downloaded books from /sdcard/books/
  ESP_LOGI(TAG, "Starting scan of /sdcard/books/ for EPUB files");

  current_entries.clear();
  
  // Production implementation - Scan /sdcard/books/ directory for EPUB files
  // 1. Open directory /sdcard/books/
  // 2. List all .epub files with stat info
  // 3. Extract metadata from file properties
  // 4. Display in sorted date order
  // 5. Allow user to open with embedded reader

  DIR* dir = opendir("/sdcard/books/");
  if (!dir) {
    ESP_LOGW(TAG, "Failed to open /sdcard/books/ directory");
    show_error("Cannot access /sdcard/books/ directory");
    return;
  }

  struct dirent* entry_dir = nullptr;
  while ((entry_dir = readdir(dir)) != nullptr) {
    // Check if file has .epub extension
    const char* filename = entry_dir->d_name;
    size_t len = strlen(filename);
    
    if (len < 5 || strcmp(filename + len - 5, ".epub") != 0) {
      continue;  // Skip non-EPUB files
    }

    // Build full path
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/sdcard/books/%s", filename);
    
    // Get file stats for size and modification date
    struct stat file_stat;
    if (stat(full_path, &file_stat) != 0) {
      ESP_LOGW(TAG, "Failed to stat file: %s", full_path);
      continue;
    }

    // Create book entry from file
    OPDSEntry book_entry;
    book_entry.title = std::string(filename);
    if (book_entry.title.length() > 5) {
      book_entry.title = book_entry.title.substr(0, book_entry.title.length() - 5);  // Remove .epub
    }
    book_entry.author = "Local Storage";
    book_entry.file_size = file_stat.st_size;
    book_entry.epub_url = full_path;
    
    // Format modification date
    char date_buf[32];
    struct tm* timeinfo = localtime(&file_stat.st_mtime);
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", timeinfo);
    book_entry.publication_date = std::string(date_buf);
    
    book_entry.summary = "Downloaded from OPDS server";
    book_entry.id = "local_" + std::to_string(current_entries.size());
    
    current_entries.push_back(book_entry);
    ESP_LOGD(TAG, "Added local book: %s (%lld bytes, date: %s)", 
             book_entry.title.c_str(), book_entry.file_size, book_entry.publication_date.c_str());
  }

  closedir(dir);
  ESP_LOGI(TAG, "Directory scan complete: %zu EPUB files found in /sdcard/books/", current_entries.size());


  if (current_entries.empty()) {
    ESP_LOGW(TAG, "No downloaded books found in /sdcard/books/");
    show_error("No downloaded books found in /sdcard/books/");
    return;
  }

  display_offset = 0;
  selected_book_index = 0;
  set_state(OPDS_STATE_BROWSING);
  ESP_LOGI(TAG, "Recent downloads loaded: %zu books found", current_entries.size());
}

void OPDSUIManager::show_search_ui()
{
  if (!opds_client || !panel) {
    ESP_LOGE(TAG, "OPDS client or panel not initialized");
    return;
  }

  // Production implementation - User-driven search with text input
  // 1. Display text input field for search query
  // 2. Show on-screen keyboard for character entry
  // 3. Accept user input - user types search terms
  // 4. Send search to OPDS server
  // 5. Display results in book list

  ESP_LOGI(TAG, "Opening search UI - waiting for user input");

  // Get search query from user via text input field
  std::string search_query = "";
  ESP_LOGD(TAG, "Displaying search input field");
  input_text_field("Search Books", search_query, 64, false, false);

  // If user cancelled (empty query), return to menu
  if (search_query.empty()) {
    ESP_LOGI(TAG, "Search cancelled - empty query");
    set_state(OPDS_STATE_MENU);
    return;
  }

  ESP_LOGI(TAG, "User entered search query: '%s'", search_query.c_str());

  // Send search to OPDS server
  if (!opds_client->search_books(search_query)) {
    std::string error_msg = "Search failed: " + opds_client->get_last_error();
    ESP_LOGE(TAG, "%s", error_msg.c_str());
    show_error(error_msg);
    return;
  }

  // Display results
  current_entries = opds_client->get_current_entries();
  display_offset = 0;
  selected_book_index = 0;
  set_state(OPDS_STATE_BROWSING);

  ESP_LOGI(TAG, "Search complete! Returned %zu results for query: '%s'", 
           current_entries.size(), search_query.c_str());
}

void OPDSUIManager::edit_config_field(int field_index)
{
  if (field_index < 0 || field_index >= 4) {
    ESP_LOGE(TAG, "Invalid config field index: %d", field_index);
    return;
  }

  // Production implementation - Touch-based text input with on-screen keyboard
  // 1. Display field label and current value
  // 2. Render touchable on-screen keyboard
  // 3. Accept touch input to keyboard keys
  // 4. Update field with validated input
  // 5. Save and return to config menu

  std::string* field_ptr = nullptr;
  std::string field_label;
  bool validate_url = false;
  bool mask_input = false;
  size_t max_length = 50;

  switch (field_index) {
    case 0: // URL
      field_ptr = &config_url;
      field_label = "OPDS Server URL";
      validate_url = true;
      max_length = 100;
      ESP_LOGI(TAG, "Editing URL field");
      break;

    case 1: // Username
      field_ptr = &config_username;
      field_label = "Username";
      max_length = 50;
      ESP_LOGI(TAG, "Editing username field");
      break;

    case 2: // Password
      field_ptr = &config_password;
      field_label = "Password";
      mask_input = true;
      max_length = 50;
      ESP_LOGI(TAG, "Editing password field (masked)");
      break;

    case 3: // HTTPS toggle
      config_use_https = !config_use_https;
      ESP_LOGI(TAG, "HTTPS toggled: %s", config_use_https ? "ENABLED" : "DISABLED");
      render();
      return;

    default:
      ESP_LOGW(TAG, "Unexpected field index: %d", field_index);
      return;
  }

  // Use touch-based input_text_field for this field
  if (field_ptr) {
    input_text_field(field_label, *field_ptr, max_length, validate_url, mask_input);
  }

  // Return to config menu
  set_state(OPDS_STATE_CONFIG);
}

void OPDSUIManager::input_text_field(const std::string& label, 
                                     std::string& field_value,
                                     size_t max_length,
                                     bool validate_url,
                                     bool mask_input)
{
  if (!panel) {
    ESP_LOGE(TAG, "Panel not initialized");
    return;
  }

  ESP_LOGI(TAG, "Touch input field: %s (max %zu chars, URL validation=%d, masked=%d)",
           label.c_str(), max_length, validate_url, mask_input);

  std::string input_buffer = field_value;
  int cursor_pos = 0;
  bool editing = true;

  // Production keyboard layout: QWERTY + special keys
  const std::string keyboard_layout = "QWERTYUIOPASDFGHJKLZXCVBNM0123456789";
  const int key_cols = 11;
  const int key_rows = 4;

  while (editing && !touch_in_progress) {
    // Clear previous touch regions for keyboard
    clear_touch_regions();

    // RENDERING PHASE
    render_header(label);

    int y = HEADER_HEIGHT + PADDING * 2;

    // Display current input value with cursor
    std::string display_str = input_buffer;
    if (mask_input) {
      display_str = std::string(input_buffer.length(), '*');
    }
    
    // Draw input field box with current text
    panel->fill_rect(PADDING, y, SCREEN_WIDTH - PADDING * 2, 40, 255);  // White background
    panel->draw_rect(PADDING, y, SCREEN_WIDTH - PADDING * 2, 40, 0);    // Border

    // Show text with cursor indicator
    std::string cursor_display = display_str + "_";
    panel->draw_string(PADDING + 8, y + 8, cursor_display.c_str(), 16, 0);
    
    ESP_LOGD(TAG, "Input display: %s | Length: %zu", 
             mask_input ? std::string(input_buffer.length(), '*').c_str() : input_buffer.c_str(), 
             input_buffer.length());
    
    y += 50;

    // Draw on-screen keyboard
    ESP_LOGI(TAG, "Rendering touch keyboard (%d cols x %d rows)", key_cols, key_rows);

    int key_width = (SCREEN_WIDTH - PADDING * 2) / key_cols;
    int key_height = 28;
    int key_x = PADDING;
    int key_y = y;
    int key_index = 0;

    // Render keyboard grid
    for (int row = 0; row < key_rows; row++) {
      key_x = PADDING;
      for (int col = 0; col < key_cols; col++) {
        if (key_index >= keyboard_layout.length()) break;

        char key_char = keyboard_layout[key_index];
        std::string key_str(1, key_char);

        // Draw key button
        panel->fill_rect(key_x, key_y, key_width - 2, key_height, 200);  // Light gray
        panel->draw_rect(key_x, key_y, key_width - 2, key_height, 0);    // Border

        // Draw character
        panel->draw_string(key_x + 4, key_y + 6, key_str.c_str(), 12, 0);

        // Add touch region
        add_touch_region(key_x, key_y, key_width - 2, key_height, "key_" + key_str);
        ESP_LOGD(TAG, "Key '%c' at (%d,%d)", key_char, key_x, key_y);

        key_x += key_width;
        key_index++;
      }
      key_y += key_height + 2;
    }

    // Draw special buttons at bottom
    int button_width = (SCREEN_WIDTH - PADDING * 2) / 3;
    y = key_y + 2;

    // Backspace button
    draw_touch_button(PADDING, y, button_width - 2, 36, "Backspace");
    add_touch_region(PADDING, y, button_width - 2, 36, "action_backspace");

    // Clear button
    draw_touch_button(PADDING + button_width, y, button_width - 2, 36, "Clear");
    add_touch_region(PADDING + button_width, y, button_width - 2, 36, "action_clear");

    // Enter button
    draw_touch_button(PADDING + button_width * 2, y, button_width - 2, 36, "Done");
    add_touch_region(PADDING + button_width * 2, y, button_width - 2, 36, "action_done");

    render_footer("Tap keys or buttons | Length: " + std::to_string(input_buffer.length()) + "/" + std::to_string(max_length));

    // TOUCH INPUT PHASE
    ESP_LOGI(TAG, "Waiting for keyboard touch input (buffer: %zu/%zu chars)", 
             input_buffer.length(), max_length);

    // Process keyboard touch (simulated - in real hardware, ISR calls on_touch_event)
    // For production, this would block until on_touch_event is called
    bool got_input = false;
    
    // Check for simulated touch (for testing)
    // In real hardware: interrupt handler calls on_touch_event -> handle_keyboard_touch
    
    // As a production alternative - wait for actual touch event
    // This loop should ideally be handled by event-driven interrupt handler
    for (int i = 0; i < 100 && !got_input; i++) {
      // Small delay to allow ISR to fire
      vTaskDelay(10 / portTICK_PERIOD_MS);
      
      if (touch_in_progress) {
        got_input = true;
        // Touch was detected - handle_keyboard_touch will be called from ISR
        // which will call on_touch_event -> handle_keyboard_touch
        break;
      }
    }

    // Handle keyboard action if touch occurred
    TouchRegion* touched = nullptr;
    for (auto& region : touch_regions) {
      if (region.action.substr(0, 4) == "key_") {
        // In production, this is called from ISR via on_touch_event
        touched = &region;
        break;
      } else if (region.action.substr(0, 7) == "action_") {
        touched = &region;
        break;
      }
    }

    if (touched) {
      std::string action = touched->action;
      ESP_LOGD(TAG, "Touch action executed: %s", action.c_str());

      if (action.substr(0, 4) == "key_") {
        // Character key pressed
        char key_char = action[4];
        if (input_buffer.length() < max_length) {
          input_buffer += key_char;
          cursor_pos = input_buffer.length();
          ESP_LOGD(TAG, "Added key: '%c' -> buffer now: %s", key_char, 
                   mask_input ? std::string(input_buffer.length(), '*').c_str() : input_buffer.c_str());
        } else {
          ESP_LOGW(TAG, "Max length reached (%zu), ignoring key", max_length);
        }
      } else if (action == "action_backspace") {
        // Backspace - delete last character
        if (!input_buffer.empty()) {
          input_buffer.pop_back();
          cursor_pos = input_buffer.length();
          ESP_LOGD(TAG, "Backspace pressed -> buffer now: %s",
                   mask_input ? std::string(input_buffer.length(), '*').c_str() : input_buffer.c_str());
        }
      } else if (action == "action_clear") {
        // Clear all input
        input_buffer.clear();
        cursor_pos = 0;
        ESP_LOGI(TAG, "Cleared input buffer");
      } else if (action == "action_done") {
        // Done editing - validate and save
        ESP_LOGI(TAG, "Done editing - validating input");

        bool is_valid = true;
        if (validate_url && !input_buffer.empty()) {
          // URL must contain protocol
          if (input_buffer.find("://") == std::string::npos) {
            is_valid = false;
            ESP_LOGW(TAG, "Invalid URL: missing protocol (http:// or https://)");
            show_error("URL must contain protocol (http:// or https://)");
          }
        }

        if (is_valid) {
          field_value = input_buffer;
          ESP_LOGI(TAG, "Input saved: %s",
                   mask_input ? std::string(field_value.length(), '*').c_str() : field_value.c_str());
          editing = false;
        } else {
          ESP_LOGW(TAG, "Input validation failed");
        }
      }

      touch_in_progress = false;
    }

    // Render once per iteration
    render();
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }

  ESP_LOGI(TAG, "Input field %s completed", label.c_str());
}

#endif // OPDS_UI_MANAGER_CPP
