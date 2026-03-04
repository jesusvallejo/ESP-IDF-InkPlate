#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "opds_client.hpp"

// Forward declarations
class M5EPaperPanel;

/**
 * @brief OPDS UI State Machine
 */
enum OPDSUIState {
  OPDS_STATE_MENU,           // Main menu
  OPDS_STATE_CONFIG,         // Configuration input
  OPDS_STATE_BROWSING,       // Browsing book catalog
  OPDS_STATE_BOOK_DETAILS,   // Showing single book details
  OPDS_STATE_DOWNLOADING,    // Download in progress
  OPDS_STATE_DOWNLOAD_COMPLETE, // Download finished
  OPDS_STATE_ERROR,          // Error state
  OPDS_STATE_CONNECT_FAILED  // Connection failed
};

/**
 * @brief OPDS UI Manager - Handles user interface for OPDS catalog browsing
 * 
 * Manages:
 * - Book catalog display
 * - Server configuration
 * - Book details and cover display
 * - Download progress visualization
 * - User input handling
 */
class OPDSUIManager {
public:
  using StateChangeCallback = std::function<void(OPDSUIState old_state, OPDSUIState new_state)>;
  using ErrorCallback = std::function<void(const std::string& error_msg)>;

  OPDSUIManager();
  ~OPDSUIManager();

  /**
   * @brief Initialize UI manager with panel reference
   */
  bool init(M5EPaperPanel* panel);

  /**
   * @brief Render current UI state
   */
  void render();

  /**
   * @brief Handle button input
   * @param button_id Button identifier (0-4 typically)
   */
  void on_button_pressed(int button_id);

  /**
   * @brief Main menu rendering
   */
  void render_main_menu();

  /**
   * @brief Configuration menu rendering
   */
  void render_config_menu();

  /**
   * @brief Book list browsing view
   */
  void render_book_list();

  /**
   * @brief Single book details view with cover
   */
  void render_book_details();

  /**
   * @brief Download progress view
   */
  void render_download_progress();

  /**
   * @brief Download complete message
   */
  void render_download_complete();

  /**
   * @brief Error display
   */
  void render_error(const std::string& error_msg);

  /**
   * @brief Update download progress from OPDS client
   */
  void update_download_progress();

  /**
   * @brief Change UI state
   */
  void set_state(OPDSUIState new_state);

  /**
   * @brief Get current state
   */
  OPDSUIState get_state() const;

  /**
   * @brief Register state change callback
   */
  void on_state_change(StateChangeCallback callback);

  /**
   * @brief Register error callback
   */
  void on_error(ErrorCallback callback);

  /**
   * @brief Process periodic updates (progress tracking, etc)
   */
  void update();

private:
  // State management
  OPDSUIState current_state;
  OPDSUIState previous_state;

  // UI component references
  M5EPaperPanel* panel;
  OPDSClient* opds_client;

  // Browse state
  std::vector<OPDSEntry> current_entries;
  size_t current_page_index;
  size_t selected_book_index;
  size_t display_offset;

  // Configuration state
  std::string config_url;
  std::string config_username;
  std::string config_password;
  bool config_use_https;
  int config_field_index;  // 0=URL, 1=username, 2=password, 3=https toggle

  // Download state
  DownloadProgress last_progress;

  // Callbacks
  std::vector<StateChangeCallback> state_change_callbacks;
  std::vector<ErrorCallback> error_callbacks;

  // Helper methods
  void fetch_catalog();
  void select_current_book();
  void start_book_download();
  void complete_download();
  void show_error(const std::string& msg);

  // UI rendering helpers
  void render_header(const std::string& title);
  void render_footer(const std::string& hint);
  void render_book_item(const OPDSEntry& entry, int y_pos, bool selected);
  void render_progress_bar(int x, int y, int width, int height, uint8_t percentage);

  // Input handling
  void handle_menu_button(int button_id);
  void handle_config_button(int button_id);
  void handle_browse_button(int button_id);
  void handle_details_button(int button_id);
  void handle_download_button(int button_id);

  // Configuration helpers
  void load_default_config();
  void save_configuration();

  // Cover image handling
  void download_and_display_cover(const OPDSEntry& entry);

  // Menu features implementation
  void show_recent_downloads();  // Browse previously downloaded books
  void show_search_ui();         // Search OPDS catalog
  void edit_config_field(int field_index); // Text input for config fields
  void input_text_field(const std::string& label,
                       std::string& field_value,
                       size_t max_length,
                       bool validate_url = false,
                       bool mask_input = false); // Generic text input UI

  // Formatting helpers
  static std::string format_size(uint64_t bytes);
  static std::string format_duration(uint32_t seconds);
};

// Global OPDS UI manager singleton
extern OPDSUIManager* g_opds_ui_manager;

#endif // OPDS_UI_MANAGER_HPP
