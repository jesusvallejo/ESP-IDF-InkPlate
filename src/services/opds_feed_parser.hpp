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

  private:
    std::vector<OPDSEntry> entries;
    std::string next_page_url;
    std::string prev_page_url;
    std::string feed_title;
    std::string last_error;

    /**
     * @brief Extract text from XML element
     * @param xml XML content
     * @param tag Tag name (without <>)
     * @param start_pos Starting position for search
     * @return Extracted text or empty string
     */
    std::string extract_tag_content(const std::string& xml, 
                                   const std::string& tag, 
                                   size_t start_pos = 0);

    /**
     * @brief Extract attribute from XML element
     * @param element XML element string
     * @param attr Attribute name
     * @return Attribute value or empty string
     */
    std::string extract_attribute(const std::string& element, const std::string& attr);
};

#endif // M5_PAPER_S3
