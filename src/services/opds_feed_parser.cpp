#include "opds_feed_parser.hpp"
#include "esp_log.h"
#include <sstream>
#include <cctype>
#include <algorithm>

static const char* TAG = "OPDSParser";

OPDSFeedParser::OPDSFeedParser()
{
}

OPDSFeedParser::~OPDSFeedParser()
{
}

bool OPDSFeedParser::parse(const std::string& xml_content)
{
  entries.clear();
  next_page_url.clear();
  prev_page_url.clear();

  return parse_xml(xml_content);
}

bool OPDSFeedParser::parse_xml(const std::string& xml_str)
{
  // Simple XML parser for OPDS feeds
  // Look for <entry> elements and extract relevant fields

  size_t pos = 0;
  while ((pos = xml_str.find("<entry", pos)) != std::string::npos) {
    size_t entry_end = xml_str.find("</entry>", pos);
    if (entry_end == std::string::npos) {
      break;
    }

    OPDSEntry entry;

    // Extract entry substring
    std::string entry_str = xml_str.substr(pos, entry_end - pos);

    // Extract ID
    if (!extract_text(entry_str, "<id>", "</id>", entry.id)) {
      ESP_LOGW(TAG, "No ID found in entry");
    }

    // Extract Title
    if (!extract_text(entry_str, "<title>", "</title>", entry.title)) {
      ESP_LOGW(TAG, "No title found in entry");
      pos = entry_end + 8;
      continue;  // Skip entry without title
    }

    // Extract Author
    extract_text(entry_str, "<author><name>", "</name></author>", entry.author);

    // Extract Summary
    extract_text(entry_str, "<summary>", "</summary>", entry.summary);

    // Extract Publication Date
    extract_text(entry_str, "<published>", "</published>", entry.publication_date);

    // Extract Cover URL (link with rel="http://opds-spec.org/image")
    extract_link(entry_str, "http://opds-spec.org/image", entry.cover_url);

    // Extract EPUB link (link with type="application/epub+zip")
    extract_link(entry_str, "application/epub+zip", entry.epub_url);

    // Extract file size from EPUB link if available
    extract_file_size(entry_str, entry.file_size);

    // Only add if we have title and EPUB URL
    if (!entry.title.empty() && !entry.epub_url.empty()) {
      entries.push_back(entry);
      ESP_LOGI(TAG, "Parsed book: %s (%s)", entry.title.c_str(), entry.author.c_str());
    }

    pos = entry_end + 8;
  }

  // Extract pagination links
  extract_pagination(xml_str);

  ESP_LOGI(TAG, "Parsed %d entries from OPDS feed", entries.size());
  return entries.size() > 0;
}

bool OPDSFeedParser::extract_text(const std::string& str,
                                   const std::string& start_tag,
                                   const std::string& end_tag,
                                   std::string& out_text)
{
  size_t start_pos = str.find(start_tag);
  if (start_pos == std::string::npos) {
    return false;
  }

  start_pos += start_tag.length();
  size_t end_pos = str.find(end_tag, start_pos);
  if (end_pos == std::string::npos) {
    return false;
  }

  out_text = str.substr(start_pos, end_pos - start_pos);
  
  // Trim and decode HTML entities
  trim(out_text);
  decode_html_entities(out_text);
  
  return !out_text.empty();
}

bool OPDSFeedParser::extract_link(const std::string& str,
                                   const std::string& rel_or_type,
                                   std::string& out_url)
{
  // Find link element with matching rel or type attribute
  size_t pos = 0;
  while ((pos = str.find("<link", pos)) != std::string::npos) {
    size_t link_end = str.find(">", pos);
    if (link_end == std::string::npos) {
      break;
    }

    std::string link_tag = str.substr(pos, link_end - pos);

    // Check if this link matches our criteria
    if (link_tag.find(rel_or_type) != std::string::npos) {
      // Extract href attribute
      size_t href_pos = link_tag.find("href=\"");
      if (href_pos != std::string::npos) {
        href_pos += 6;  // Length of "href=\""
        size_t href_end = link_tag.find("\"", href_pos);
        if (href_end != std::string::npos) {
          out_url = link_tag.substr(href_pos, href_end - href_pos);
          return true;
        }
      }
    }

    pos = link_end + 1;
  }

  return false;
}

bool OPDSFeedParser::extract_file_size(const std::string& str, uint64_t& out_size)
{
  // Look for DCTerms issued size attribute in link elements
  std::string size_str;
  if (extract_text(str, "opds:filesize=\"", "\"", size_str)) {
    out_size = std::stoull(size_str);
    return true;
  }

  // Alternative: look for opds:filesize element
  if (extract_text(str, "<opds:filesize>", "</opds:filesize>", size_str)) {
    out_size = std::stoull(size_str);
    return true;
  }

  return false;
}

void OPDSFeedParser::extract_pagination(const std::string& xml_str)
{
  // Look for rel="next" link
  size_t pos = 0;
  while ((pos = xml_str.find("rel=\"next\"", pos)) != std::string::npos) {
    // Find the beginning of this link tag
    size_t link_start = xml_str.rfind("<link", pos);
    if (link_start != std::string::npos) {
      size_t link_end = xml_str.find(">", pos);
      if (link_end != std::string::npos) {
        std::string link_tag = xml_str.substr(link_start, link_end - link_start);
        size_t href_pos = link_tag.find("href=\"");
        if (href_pos != std::string::npos) {
          href_pos += 6;
          size_t href_end = link_tag.find("\"", href_pos);
          if (href_end != std::string::npos) {
            next_page_url = link_tag.substr(href_pos, href_end - href_pos);
            ESP_LOGI(TAG, "Found next page: %s", next_page_url.c_str());
          }
        }
      }
    }
    pos++;
  }

  // Look for rel="previous" link
  pos = 0;
  while ((pos = xml_str.find("rel=\"previous\"", pos)) != std::string::npos) {
    size_t link_start = xml_str.rfind("<link", pos);
    if (link_start != std::string::npos) {
      size_t link_end = xml_str.find(">", pos);
      if (link_end != std::string::npos) {
        std::string link_tag = xml_str.substr(link_start, link_end - link_start);
        size_t href_pos = link_tag.find("href=\"");
        if (href_pos != std::string::npos) {
          href_pos += 6;
          size_t href_end = link_tag.find("\"", href_pos);
          if (href_end != std::string::npos) {
            prev_page_url = link_tag.substr(href_pos, href_end - href_pos);
            ESP_LOGI(TAG, "Found previous page: %s", prev_page_url.c_str());
          }
        }
      }
    }
    pos++;
  }
}

void OPDSFeedParser::trim(std::string& str)
{
  // Remove leading whitespace
  str.erase(str.begin(), std::find_if(str.begin(), str.end(),
                                      [](unsigned char ch) { return !std::isspace(ch); }));

  // Remove trailing whitespace
  str.erase(std::find_if(str.rbegin(), str.rend(),
                        [](unsigned char ch) { return !std::isspace(ch); }).base(),
            str.end());
}

void OPDSFeedParser::decode_html_entities(std::string& str)
{
  // Simple HTML entity decoder
  size_t pos = 0;
  while ((pos = str.find("&", pos)) != std::string::npos) {
    size_t end = str.find(";", pos + 1);
    if (end == std::string::npos) {
      break;
    }

    std::string entity = str.substr(pos, end - pos + 1);

    if (entity == "&lt;") {
      str.replace(pos, entity.length(), "<");
    } else if (entity == "&gt;") {
      str.replace(pos, entity.length(), ">");
    } else if (entity == "&amp;") {
      str.replace(pos, entity.length(), "&");
    } else if (entity == "&quot;") {
      str.replace(pos, entity.length(), "\"");
    } else if (entity == "&apos;") {
      str.replace(pos, entity.length(), "'");
    } else {
      pos++;
      continue;
    }

    pos++;
  }
}

const OPDSEntry* OPDSFeedParser::get_entry(size_t index) const
{
  if (index < entries.size()) {
    return &entries[index];
  }
  return nullptr;
}

void OPDSFeedParser::clear()
{
  entries.clear();
  next_page_url.clear();
  prev_page_url.clear();
  feed_title.clear();
  last_error.clear();
}
