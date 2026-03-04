#pragma once

#include <string>
#include <vector>

// Forward declarations
class M5EPaperPanel;

/**
 * @brief Touch region for hit detection
 */
struct TouchRegion {
  int x, y, width, height;
  std::string action;  // "menu_1", "config_url", "keyboard_A", etc.
};

/**
 * @brief UI Helper functions for OPDS UI Manager
 * 
 * Provides drawing and touch management utilities for the OPDS interface
 */
class OPDSUIHelpers {
public:
  /**
   * @brief Draw a touchable button on screen
   */
  static void draw_touch_button(M5EPaperPanel* panel, int x, int y, int width, int height, 
                               const std::string& label, bool highlighted = false);

  /**
   * @brief Draw full on-screen QWERTY keyboard
   */
  static void draw_on_screen_keyboard(M5EPaperPanel* panel, int x, int y);

  /**
   * @brief Draw individual keyboard key
   */
  static void render_keyboard_letter(M5EPaperPanel* panel, int col, int row, char letter, 
                                     int x, int y, bool pressed = false);

  /**
   * @brief Check if point is within touch region
   */
  static TouchRegion* get_touched_region(std::vector<TouchRegion>& regions, int x, int y);

  /**
   * @brief Clear all touch regions
   */
  static void clear_touch_regions(std::vector<TouchRegion>& regions);

  /**
   * @brief Add new touch region
   */
  static void add_touch_region(std::vector<TouchRegion>& regions, int x, int y, 
                               int width, int height, const std::string& action);

private:
  static const int SCREEN_WIDTH;
  static const int SCREEN_HEIGHT;
  static const int PADDING;
};

#endif // OPDS_UI_HELPERS_HPP
