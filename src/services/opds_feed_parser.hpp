#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

/**
 * @brief OPDS Book Entry
 * 
 * Represents a single book in an OPDS feed.
 */
struct OPDSEntry
{
  std::string id;           // Unique book ID
  std::string title;        // Book title
  std::string author;       // Book author(s)
  std::string summary;      // Book description/summary
  std::string cover_url;    // Cover image URL
  std::string epub_url;     // EPUB download URL
  uint64_t file_size;       // File size in bytes (0 if unknown)
  std::string publication_date;
};

/**
 * @brief OPDS Feed Parser
 * 
 * Parses OPDS (Open Publication Distribution System) XML feeds.
 * Can handle catalog feeds and search results.
 */
class OPDSFeedParser
{
  public:
    OPDSFeedParser();
    ~OPDSFeedParser();

    /**
     * @brief Parse OPDS XML feed
     * @param xml_content XML feed content as string
     * @return true if parsed successfully
     */
    bool parse(const std::string& xml_content);

    /**
     * @brief Get parsed book entries
     * @return Vector of book entries
     */
    const std::vector<OPDSEntry>& get_entries() const { return entries; }

    /**
     * @brief Get number of books in current feed
     * @return Entry count
     */
    size_t get_entry_count() const { return entries.size(); }

    /**
     * @brief Get entry by index
     * @param index Entry index
     * @return Pointer to entry or nullptr if out of range
     */
    const OPDSEntry* get_entry(size_t index) const;

    /**
     * @brief Get next page URL if available
     * @return URL to next page or empty string
     */
    std::string get_next_page_url() const { return next_page_url; }

    /**
     * @brief Get previous page URL if available
     * @return URL to previous page or empty string
     */
    std::string get_prev_page_url() const { return prev_page_url; }

    /**
     * @brief Get feed title/category name
     * @return Feed title
     */
    std::string get_feed_title() const { return feed_title; }

    /**
     * @brief Clear parsed data
     */
    void clear();

    /**
     * @brief Get last parsing error
     * @return Error message
     */
    std::string get_last_error() const { return last_error; }

    /**
     * @brief Check if next page is available
     * @return true if next page URL exists
     */
    bool has_next() const { return !next_page_url.empty(); }

    /**
     * @brief Check if previous page is available
     * @return true if previous page URL exists
     */
    bool has_prev() const { return !prev_page_url.empty(); }

  private:
    std::vector<OPDSEntry> entries;
    std::string next_page_url;
    std::string prev_page_url;
    std::string feed_title;
    std::string last_error;

    /**
     * @brief Parse XML feed content
     * @param xml_str XML feed content
     * @return true if parsed successfully
     */
    bool parse_xml(const std::string& xml_str);

    /**
     * @brief Extract text content between XML tags
     * @param str Input string
     * @param start_tag Opening tag
     * @param end_tag Closing tag
     * @param out_text Extracted text
     * @return true if text was found
     */
    bool extract_text(const std::string& str,
                     const std::string& start_tag,
                     const std::string& end_tag,
                     std::string& out_text);

    /**
     * @brief Extract link from OPDS entry
     * @param str Entry string
     * @param rel Link relationship/type
     * @param out_url Extracted URL
     * @return true if link was found
     */
    bool extract_link(const std::string& str,
                     const std::string& rel,
                     std::string& out_url);

    /**
     * @brief Extract file size from link attributes
     * @param str Entry or link string
     * @param out_size Extracted file size
     * @return true if size was found
     */
    bool extract_file_size(const std::string& str, uint64_t& out_size);

    /**
     * @brief Extract pagination links from feed
     * @param xml_str Full feed XML
     */
    void extract_pagination(const std::string& xml_str);

    /**
     * @brief Trim whitespace from string
     * @param str String to trim
     */
    void trim(std::string& str);

    /**
     * @brief Decode HTML entities in string
     * @param str String to decode
     */
    void decode_html_entities(std::string& str);
};
