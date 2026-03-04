#include "opds_ui_helpers.hpp"
#include "esp_log.h"
#include "../../../../src/helpers/opds_display_adapter.hpp"

static const char* TAG = "OPDSUIHelpers";

const int OPDSUIHelpers::SCREEN_WIDTH = 960;
const int OPDSUIHelpers::SCREEN_HEIGHT = 540;
const int OPDSUIHelpers::PADDING = 16;

void OPDSUIHelpers::draw_touch_button(M5EPaperPanel* panel, int x, int y, int width, 
                                      int height, const std::string& label, bool highlighted)
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

  ESP_LOGD(TAG, "Button drawn: '%s' at (%d,%d) size %dx%d", label.c_str(), x, y, width, height);
}

void OPDSUIHelpers::draw_on_screen_keyboard(M5EPaperPanel* panel, int x, int y)
{
  if (!panel) return;

  // Keyboard layout: QWERTY style - 4 rows
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
      render_keyboard_letter(panel, col_x, row_y, c, col_x, row_y);
      col_x += 60;  // Key spacing
    }
    row_y += 32;  // Row spacing
  }

  ESP_LOGD(TAG, "On-screen keyboard rendered at (%d,%d)", x, y);
}

void OPDSUIHelpers::render_keyboard_letter(M5EPaperPanel* panel, int col, int row, 
                                           char letter, int x, int y, bool pressed)
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
    ESP_LOGD(TAG, "Keyboard key '%c' rendered pressed at (%d,%d)", letter, x, y);
  }
}

TouchRegion* OPDSUIHelpers::get_touched_region(std::vector<TouchRegion>& regions, int x, int y)
{
  for (auto& region : regions) {
    if (x >= region.x && x <= region.x + region.width &&
        y >= region.y && y <= region.y + region.height) {
      ESP_LOGD(TAG, "Touch hit region: %s at (%d,%d)", region.action.c_str(), x, y);
      return &region;
    }
  }
  return nullptr;
}

void OPDSUIHelpers::clear_touch_regions(std::vector<TouchRegion>& regions)
{
  ESP_LOGD(TAG, "Clearing %zu touch regions", regions.size());
  regions.clear();
}

void OPDSUIHelpers::add_touch_region(std::vector<TouchRegion>& regions, int x, int y, 
                                     int width, int height, const std::string& action)
{
  regions.push_back({x, y, width, height, action});
  ESP_LOGD(TAG, "Added touch region: (%d,%d) %dx%d action='%s'", x, y, width, height, action.c_str());
}
