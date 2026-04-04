#include "browser_fetch.h"
#include "browser_image_decode.h"
#include "browser_platform.h"

#include <litehtml.h>
#include <litehtml/url.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace
{

constexpr int BROWSER_WIDTH = 1024;
constexpr int BROWSER_HEIGHT = 720;
constexpr int BROWSER_LOOP_SLEEP_MS = 16;
constexpr int BROWSER_HEADER_HEIGHT = 76;
constexpr int BROWSER_MARGIN = 14;
constexpr int BROWSER_INPUT_X = 94;
constexpr int BROWSER_INPUT_Y = 16;
constexpr int BROWSER_INPUT_HEIGHT = 28;
constexpr int BROWSER_SCROLL_STEP = 48;
constexpr int BROWSER_TEXT_LINE_HEIGHT = 20;
constexpr bool BROWSER_ENABLE_LITEHTML = true;

constexpr std::uint32_t COLOR_BG = 0xe7ecefff;
constexpr std::uint32_t COLOR_PANEL = 0xfafaf9ff;
constexpr std::uint32_t COLOR_HEADER = 0x16324fff;
constexpr std::uint32_t COLOR_INPUT = 0xffffffff;
constexpr std::uint32_t COLOR_TEXT = 0x1f2933ff;
constexpr std::uint32_t COLOR_MUTED = 0xc7d2daff;
constexpr std::uint32_t COLOR_STATUS = 0xb8f2e6ff;
constexpr std::uint32_t COLOR_ERROR = 0xc1121fff;
constexpr std::uint32_t COLOR_LINK = 0x1976d2ff;

static std::uint32_t litehtml_color(const litehtml::web_color &color)
{
    return xj380_rgba(color.red, color.green, color.blue, color.alpha);
}

static std::string lower_copy(const std::string &value)
{
    std::string lowered = value;
    for (char &ch : lowered) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lowered;
}

static std::string trim_copy(const std::string &value)
{
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return value.substr(start, end - start);
}

static std::string shorten_url_for_log(const std::string &url)
{
    constexpr std::size_t kMaxLogUrlBytes = 120;
    if (url.size() <= kMaxLogUrlBytes) {
        return url;
    }
    return url.substr(0, kMaxLogUrlBytes) + "...";
}

static void log_line(const std::string &text)
{
    char buffer[512];
    std::snprintf(buffer, sizeof(buffer), "browser: %s\n", text.c_str());
    xapi_OutputSerial(buffer);
}

static std::size_t find_case_insensitive(const std::string &text, const std::string &needle, std::size_t start = 0)
{
    if (needle.empty() || start >= text.size()) {
        return std::string::npos;
    }

    auto it = std::search(text.begin() + static_cast<std::ptrdiff_t>(start),
                          text.end(),
                          needle.begin(),
                          needle.end(),
                          [](char lhs, char rhs) {
                              return std::tolower(static_cast<unsigned char>(lhs)) ==
                                     std::tolower(static_cast<unsigned char>(rhs));
                          });
    if (it == text.end()) {
        return std::string::npos;
    }
    return static_cast<std::size_t>(it - text.begin());
}

static void erase_tag_block(std::string &html, const char *tag_name)
{
    const std::string open_tag = std::string("<") + tag_name;
    const std::string close_tag = std::string("</") + tag_name;

    std::size_t cursor = 0;
    while (cursor < html.size()) {
        const std::size_t open_pos = find_case_insensitive(html, open_tag, cursor);
        if (open_pos == std::string::npos) {
            break;
        }

        const std::size_t close_pos = find_case_insensitive(html, close_tag, open_pos + open_tag.size());
        if (close_pos == std::string::npos) {
            html.erase(open_pos);
            break;
        }

        const std::size_t close_end = html.find('>', close_pos);
        if (close_end == std::string::npos) {
            html.erase(open_pos);
            break;
        }

        html.erase(open_pos, close_end - open_pos + 1);
        cursor = open_pos;
    }
}

static void erase_tag_markers(std::string &html, const char *tag_name)
{
    const std::string open_tag = std::string("<") + tag_name;
    const std::string close_tag = std::string("</") + tag_name;

    std::size_t cursor = 0;
    while (cursor < html.size()) {
        const std::size_t open_pos = find_case_insensitive(html, open_tag, cursor);
        if (open_pos == std::string::npos) {
            break;
        }
        const std::size_t open_end = html.find('>', open_pos);
        if (open_end == std::string::npos) {
            break;
        }
        html.erase(open_pos, open_end - open_pos + 1);
        cursor = open_pos;
    }

    cursor = 0;
    while (cursor < html.size()) {
        const std::size_t close_pos = find_case_insensitive(html, close_tag, cursor);
        if (close_pos == std::string::npos) {
            break;
        }
        const std::size_t close_end = html.find('>', close_pos);
        if (close_end == std::string::npos) {
            break;
        }
        html.erase(close_pos, close_end - close_pos + 1);
        cursor = close_pos;
    }
}

static std::string escape_html_text(const std::string &text)
{
    std::string escaped;
    escaped.reserve(text.size());
    for (unsigned char ch : text) {
        switch (ch) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        default:
            if (ch == '\r') {
                continue;
            }
            escaped.push_back(static_cast<char>(ch));
            break;
        }
    }
    return escaped;
}

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static bool is_identifier_char(char ch)
{
    return (ch >= '0' && ch <= '9') ||
           (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           ch == '_' ||
           ch == '$';
}

static std::size_t skip_js_space(const std::string &text, std::size_t pos)
{
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
    return pos;
}

static bool decode_js_string_literal(const std::string &text,
                                     std::size_t start,
                                     std::string &value,
                                     std::size_t &next)
{
    value.clear();
    if (start >= text.size()) {
        return false;
    }

    const char quote = text[start];
    if (quote != '"' && quote != '\'' && quote != '`') {
        return false;
    }

    std::size_t pos = start + 1;
    while (pos < text.size()) {
        const char ch = text[pos++];
        if (ch == quote) {
            next = pos;
            return true;
        }
        if (ch != '\\') {
            if (quote == '`' && ch == '$' && pos < text.size() && text[pos] == '{') {
                return false;
            }
            value.push_back(ch);
            continue;
        }
        if (pos >= text.size()) {
            return false;
        }

        const char escaped = text[pos++];
        switch (escaped) {
        case '\\':
        case '\'':
        case '"':
        case '`':
            value.push_back(escaped);
            break;
        case 'n':
            value.push_back('\n');
            break;
        case 'r':
            value.push_back('\r');
            break;
        case 't':
            value.push_back('\t');
            break;
        case 'x':
            if (pos + 1 > text.size()) {
                return false;
            } else {
                const int hi = hex_value(text[pos]);
                const int lo = hex_value(text[pos + 1]);
                if (hi < 0 || lo < 0) {
                    return false;
                }
                value.push_back(static_cast<char>((hi << 4) | lo));
                pos += 2;
            }
            break;
        case 'u':
            if (pos + 3 >= text.size()) {
                return false;
            } else {
                int codepoint = 0;
                for (int i = 0; i < 4; ++i) {
                    const int digit = hex_value(text[pos + i]);
                    if (digit < 0) {
                        return false;
                    }
                    codepoint = (codepoint << 4) | digit;
                }
                pos += 4;
                if (codepoint <= 0x7f) {
                    value.push_back(static_cast<char>(codepoint));
                }
            }
            break;
        default:
            value.push_back(escaped);
            break;
        }
    }

    return false;
}

static bool parse_js_string_expression(const std::string &text,
                                       std::size_t start,
                                       std::string &value,
                                       std::size_t &next)
{
    value.clear();
    std::size_t pos = skip_js_space(text, start);

    std::string part;
    std::size_t part_end = pos;
    if (!decode_js_string_literal(text, pos, part, part_end)) {
        return false;
    }

    value += part;
    pos = skip_js_space(text, part_end);
    while (pos < text.size() && text[pos] == '+') {
        pos = skip_js_space(text, pos + 1);
        if (!decode_js_string_literal(text, pos, part, part_end)) {
            return false;
        }
        value += part;
        pos = skip_js_space(text, part_end);
    }

    next = pos;
    return true;
}

static std::string extract_tag_attribute(const std::string &tag, const std::string &name)
{
    std::size_t pos = 0;
    while (pos < tag.size()) {
        pos = find_case_insensitive(tag, name, pos);
        if (pos == std::string::npos) {
            break;
        }
        if (pos > 0 && is_identifier_char(tag[pos - 1])) {
            pos += name.size();
            continue;
        }

        std::size_t value_pos = skip_js_space(tag, pos + name.size());
        if (value_pos >= tag.size() || tag[value_pos] != '=') {
            pos += name.size();
            continue;
        }

        value_pos = skip_js_space(tag, value_pos + 1);
        if (value_pos >= tag.size()) {
            return std::string();
        }

        if (tag[value_pos] == '"' || tag[value_pos] == '\'') {
            const char quote = tag[value_pos++];
            const std::size_t value_end = tag.find(quote, value_pos);
            if (value_end == std::string::npos) {
                return tag.substr(value_pos);
            }
            return tag.substr(value_pos, value_end - value_pos);
        }

        std::size_t value_end = value_pos;
        while (value_end < tag.size() &&
               std::isspace(static_cast<unsigned char>(tag[value_end])) == 0 &&
               tag[value_end] != '>') {
            ++value_end;
        }
        return tag.substr(value_pos, value_end - value_pos);
    }

    return std::string();
}

static bool is_executable_script_type(const std::string &type)
{
    const std::string lowered = lower_copy(trim_copy(type));
    if (lowered.empty()) {
        return true;
    }
    return lowered.find("javascript") != std::string::npos ||
           lowered.find("ecmascript") != std::string::npos ||
           lowered == "module" ||
           lowered == "text/module";
}

static bool find_js_call(const std::string &source,
                         const std::string &needle,
                         std::size_t start,
                         std::size_t &call_pos,
                         std::size_t &arg_pos)
{
    std::size_t pos = start;
    while (true) {
        pos = find_case_insensitive(source, needle, pos);
        if (pos == std::string::npos) {
            return false;
        }
        if (pos > 0 && is_identifier_char(source[pos - 1])) {
            pos += needle.size();
            continue;
        }

        std::size_t open_pos = skip_js_space(source, pos + needle.size());
        if (open_pos < source.size() && source[open_pos] == '(') {
            call_pos = pos;
            arg_pos = open_pos + 1;
            return true;
        }
        pos += needle.size();
    }
}

static bool find_js_assignment(const std::string &source,
                               const std::string &needle,
                               std::size_t start,
                               std::size_t &assign_pos,
                               std::size_t &value_pos)
{
    std::size_t pos = start;
    while (true) {
        pos = find_case_insensitive(source, needle, pos);
        if (pos == std::string::npos) {
            return false;
        }
        if (pos > 0 && is_identifier_char(source[pos - 1])) {
            pos += needle.size();
            continue;
        }

        std::size_t equal_pos = skip_js_space(source, pos + needle.size());
        if (equal_pos < source.size() && source[equal_pos] == '=') {
            assign_pos = pos;
            value_pos = skip_js_space(source, equal_pos + 1);
            return true;
        }
        pos += needle.size();
    }
}

struct BrowserPreparedDocument
{
    bool        fallback_used = false;
    std::string html;
    std::string redirect_url;
    std::string title_override;
    int         script_blocks = 0;
    int         scripts_with_effect = 0;
    int         document_writes = 0;
    int         external_scripts = 0;
};

static void set_html_title(std::string &html, const std::string &title)
{
    if (title.empty()) {
        return;
    }

    const std::string escaped = escape_html_text(title);
    const std::size_t title_pos = find_case_insensitive(html, "<title");
    if (title_pos != std::string::npos) {
        const std::size_t title_open_end = html.find('>', title_pos);
        const std::size_t title_close_pos = find_case_insensitive(html, "</title", title_open_end == std::string::npos ? title_pos : title_open_end + 1);
        if (title_open_end != std::string::npos && title_close_pos != std::string::npos) {
            html.replace(title_open_end + 1, title_close_pos - title_open_end - 1, escaped);
            return;
        }
    }

    const std::string title_tag = "<title>" + escaped + "</title>";
    const std::size_t head_pos = find_case_insensitive(html, "<head");
    if (head_pos != std::string::npos) {
        const std::size_t head_end = html.find('>', head_pos);
        if (head_end != std::string::npos) {
            html.insert(head_end + 1, title_tag);
            return;
        }
    }

    html.insert(0, "<head>" + title_tag + "</head>");
}

static bool percent_decode(const std::string &input, std::string &output)
{
    output.clear();
    output.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        if (ch == '%') {
            if (i + 2 >= input.size()) {
                return false;
            }
            const int hi = hex_value(input[i + 1]);
            const int lo = hex_value(input[i + 2]);
            if (hi < 0 || lo < 0) {
                return false;
            }
            output.push_back(static_cast<char>((hi << 4) | lo));
            i += 2;
            continue;
        }
        output.push_back(ch == '+' ? ' ' : ch);
    }
    return true;
}

static int base64_value(char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A';
    }
    if (ch >= 'a' && ch <= 'z') {
        return ch - 'a' + 26;
    }
    if (ch >= '0' && ch <= '9') {
        return ch - '0' + 52;
    }
    if (ch == '+') {
        return 62;
    }
    if (ch == '/') {
        return 63;
    }
    return -1;
}

static bool decode_base64(const std::string &input, std::string &output)
{
    output.clear();
    output.reserve((input.size() * 3) / 4);

    int accum = 0;
    int bits = 0;
    for (char ch : input) {
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            continue;
        }
        if (ch == '=') {
            break;
        }
        const int value = base64_value(ch);
        if (value < 0) {
            return false;
        }

        accum = (accum << 6) | value;
        bits += 6;
        while (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<char>((accum >> bits) & 0xff));
        }
    }
    return true;
}

static bool decode_data_url(const std::string &url, BrowserFetchResult &result)
{
    const std::string lowered = lower_copy(url);
    if (lowered.rfind("data:", 0) != 0) {
        return false;
    }

    const std::size_t comma = url.find(',');
    if (comma == std::string::npos) {
        result.ok = false;
        result.error = "invalid data url";
        return true;
    }

    const std::string meta = url.substr(5, comma - 5);
    const std::string payload = url.substr(comma + 1);
    const std::string lowered_meta = lower_copy(meta);
    const bool is_base64 = lowered_meta.find(";base64") != std::string::npos;

    std::string decoded;
    const bool ok = is_base64 ? decode_base64(payload, decoded) : percent_decode(payload, decoded);
    if (!ok) {
        result.ok = false;
        result.error = is_base64 ? "bad data url base64" : "bad data url escape";
        return true;
    }

    std::string content_type = "text/plain;charset=US-ASCII";
    if (!meta.empty()) {
        const std::size_t semi = meta.find(';');
        const std::string explicit_type = trim_copy(meta.substr(0, semi));
        if (!explicit_type.empty()) {
            content_type = explicit_type;
        }
    }

    result.ok = true;
    result.status_code = 200;
    result.final_url = url;
    result.content_type = content_type;
    result.body = std::move(decoded);
    return true;
}

static bool looks_like_html(const BrowserFetchResult &response)
{
    const std::string content_type = lower_copy(response.content_type);
    if (content_type.find("text/html") != std::string::npos ||
        content_type.find("application/xhtml+xml") != std::string::npos) {
        return true;
    }

    const std::size_t probe_len = std::min<std::size_t>(response.body.size(), 1024);
    const std::string probe = response.body.substr(0, probe_len);
    return find_case_insensitive(probe, "<html") != std::string::npos ||
           find_case_insensitive(probe, "<!doctype html") != std::string::npos ||
           find_case_insensitive(probe, "<body") != std::string::npos;
}

static std::string build_fallback_document(const std::string &title,
                                           const std::string &message,
                                           const std::string &excerpt)
{
    std::string html;
    html.reserve(excerpt.size() + 512);
    html += "<html><head><meta charset=\"utf-8\"><title>";
    html += escape_html_text(title);
    html += "</title></head><body style=\"font-family:sans-serif;background:#fafaf9;color:#1f2933;padding:24px;\">";
    html += "<h1 style=\"font-size:24px;margin:0 0 12px 0;\">";
    html += escape_html_text(title);
    html += "</h1><p style=\"margin:0 0 16px 0;\">";
    html += escape_html_text(message);
    html += "</p><pre style=\"white-space:pre-wrap;word-break:break-word;background:#ffffff;border:1px solid #c7d2da;padding:12px;\">";
    html += escape_html_text(excerpt);
    html += "</pre></body></html>";
    return html;
}

static std::string decode_basic_entities(const std::string &text)
{
    std::string decoded;
    decoded.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '&') {
            decoded.push_back(text[i]);
            continue;
        }

        if (text.compare(i, 5, "&amp;") == 0) {
            decoded.push_back('&');
            i += 4;
        } else if (text.compare(i, 4, "&lt;") == 0) {
            decoded.push_back('<');
            i += 3;
        } else if (text.compare(i, 4, "&gt;") == 0) {
            decoded.push_back('>');
            i += 3;
        } else if (text.compare(i, 6, "&quot;") == 0) {
            decoded.push_back('"');
            i += 5;
        } else if (text.compare(i, 5, "&#39;") == 0) {
            decoded.push_back('\'');
            i += 4;
        } else if (text.compare(i, 6, "&nbsp;") == 0) {
            decoded.push_back(' ');
            i += 5;
        } else {
            decoded.push_back('&');
        }
    }

    return decoded;
}

static std::string html_to_text(std::string html)
{
    erase_tag_block(html, "script");
    erase_tag_block(html, "style");
    erase_tag_markers(html, "noscript");

    std::string text;
    text.reserve(html.size());

    bool in_tag = false;
    bool last_was_space = false;
    for (std::size_t i = 0; i < html.size(); ++i) {
        const char ch = html[i];

        if (!in_tag && ch == '<') {
            const std::size_t tag_end = html.find('>', i);
            const std::size_t tag_text_start = i + 1;
            const std::size_t tag_text_len = (tag_end == std::string::npos ? html.size() : tag_end) - tag_text_start;
            const std::string tag = lower_copy(trim_copy(html.substr(tag_text_start, tag_text_len)));

            if (tag.rfind("br", 0) == 0 || tag == "/p" || tag == "p" || tag == "/div" || tag == "div" ||
                tag == "/li" || tag == "li" || tag == "/h1" || tag == "h1" || tag == "/h2" || tag == "h2" ||
                tag == "/h3" || tag == "h3" || tag == "hr") {
                if (text.empty() || text.back() != '\n') {
                    text.push_back('\n');
                }
                last_was_space = false;
            }

            in_tag = true;
            continue;
        }

        if (in_tag) {
            if (ch == '>') {
                in_tag = false;
            }
            continue;
        }

        if (ch == '\r') {
            continue;
        }
        if (ch == '\n' || ch == '\t') {
            if (!text.empty() && text.back() != '\n') {
                text.push_back('\n');
            }
            last_was_space = false;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            if (!last_was_space && !text.empty() && text.back() != '\n') {
                text.push_back(' ');
                last_was_space = true;
            }
            continue;
        }

        text.push_back(ch);
        last_was_space = false;
    }

    return decode_basic_entities(text);
}

static std::vector<std::string> wrap_text_for_width(const std::string &text, int pixel_width)
{
    std::vector<std::string> lines;
    std::string current_line;

    const auto flush_line = [&]() {
        lines.push_back(current_line);
        current_line.clear();
    };

    std::size_t cursor = 0;
    while (cursor <= text.size()) {
        std::size_t line_end = text.find('\n', cursor);
        const bool explicit_break = line_end != std::string::npos;
        const std::string raw_line = explicit_break ? text.substr(cursor, line_end - cursor) : text.substr(cursor);

        if (raw_line.empty()) {
            lines.emplace_back();
        } else {
            std::size_t start = 0;
            while (start < raw_line.size()) {
                while (start < raw_line.size() && raw_line[start] == ' ') {
                    ++start;
                }
                if (start >= raw_line.size()) {
                    break;
                }

                std::size_t end = start;
                std::string candidate;
                std::size_t last_fit = start;

                while (end <= raw_line.size()) {
                    const std::string part = raw_line.substr(start, end - start);
                    const unsigned long long width = xapi_CalcTextWidth(const_cast<char *>(part.c_str()), 16);
                    if (width > static_cast<unsigned long long>(pixel_width) && !candidate.empty()) {
                        break;
                    }
                    if (width <= static_cast<unsigned long long>(pixel_width)) {
                        candidate = part;
                        last_fit = end;
                    }

                    std::size_t next_space = raw_line.find(' ', end);
                    if (next_space == std::string::npos) {
                        end = raw_line.size();
                    } else if (next_space == end) {
                        ++end;
                    } else {
                        end = next_space;
                    }

                    if (end >= raw_line.size()) {
                        const std::string tail = raw_line.substr(start);
                        const unsigned long long tail_width = xapi_CalcTextWidth(const_cast<char *>(tail.c_str()), 16);
                        if (tail_width <= static_cast<unsigned long long>(pixel_width)) {
                            candidate = tail;
                            last_fit = raw_line.size();
                        }
                        break;
                    }
                }

                if (candidate.empty()) {
                    std::size_t hard_end = start + 1;
                    while (hard_end <= raw_line.size()) {
                        const std::string part = raw_line.substr(start, hard_end - start);
                        const unsigned long long width = xapi_CalcTextWidth(const_cast<char *>(part.c_str()), 16);
                        if (width > static_cast<unsigned long long>(pixel_width) && hard_end > start + 1) {
                            --hard_end;
                            break;
                        }
                        if (hard_end == raw_line.size()) {
                            break;
                        }
                        ++hard_end;
                    }
                    candidate = raw_line.substr(start, hard_end - start);
                    last_fit = hard_end;
                }

                current_line = trim_copy(candidate);
                flush_line();
                start = last_fit;
            }
        }

        if (!explicit_break) {
            break;
        }
        cursor = line_end + 1;
    }

    if (lines.empty()) {
        lines.emplace_back("No content.");
    }

    return lines;
}

class BrowserApp;

class BrowserContainer final : public litehtml::document_container
{
public:
    explicit BrowserContainer(BrowserApp &app) : app_(app) {}

    litehtml::uint_ptr create_font(const litehtml::font_description &descr,
                                   const litehtml::document *,
                                   litehtml::font_metrics *fm) override;
    void delete_font(litehtml::uint_ptr hFont) override;
    litehtml::pixel_t text_width(const char *text, litehtml::uint_ptr hFont) override;
    void draw_text(litehtml::uint_ptr hdc,
                   const char *text,
                   litehtml::uint_ptr hFont,
                   litehtml::web_color color,
                   const litehtml::position &pos) override;
    litehtml::pixel_t pt_to_px(float pt) const override { return pt * 96.0f / 72.0f; }
    litehtml::pixel_t get_default_font_size() const override { return 16.0f; }
    const char *get_default_font_name() const override { return "sans"; }
    void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker &marker) override;
    void load_image(const char *src, const char *baseurl, bool redraw_on_ready) override;
    void get_image_size(const char *src, const char *baseurl, litehtml::size &sz) override;
    void draw_image(litehtml::uint_ptr hdc,
                    const litehtml::background_layer &layer,
                    const std::string &url,
                    const std::string &base_url) override;
    void draw_solid_fill(litehtml::uint_ptr hdc, const litehtml::background_layer &layer, const litehtml::web_color &color) override;
    void draw_linear_gradient(litehtml::uint_ptr hdc,
                              const litehtml::background_layer &layer,
                              const litehtml::background_layer::linear_gradient &gradient) override;
    void draw_radial_gradient(litehtml::uint_ptr hdc,
                              const litehtml::background_layer &layer,
                              const litehtml::background_layer::radial_gradient &gradient) override;
    void draw_conic_gradient(litehtml::uint_ptr hdc,
                             const litehtml::background_layer &layer,
                             const litehtml::background_layer::conic_gradient &gradient) override;
    void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders &borders, const litehtml::position &draw_pos, bool root) override;

    void set_caption(const char *caption) override;
    void set_base_url(const char *base_url) override;
    void link(const std::shared_ptr<litehtml::document> &, const litehtml::element::ptr &) override {}
    void on_anchor_click(const char *url, const litehtml::element::ptr &) override;
    void on_mouse_event(const litehtml::element::ptr &, litehtml::mouse_event) override {}
    void set_cursor(const char *) override {}
    void transform_text(litehtml::string &text, litehtml::text_transform tt) override;
    void import_css(litehtml::string &text, const litehtml::string &url, litehtml::string &baseurl) override;
    void set_clip(const litehtml::position &pos, const litehtml::border_radiuses &) override;
    void del_clip() override;
    void get_viewport(litehtml::position &viewport) const override { viewport = viewport_; }
    litehtml::element::ptr create_element(const char *, const litehtml::string_map &, const std::shared_ptr<litehtml::document> &) override
    {
        return nullptr;
    }

    void get_media_features(litehtml::media_features &media) const override;
    void get_language(litehtml::string &language, litehtml::string &culture) const override;

    void set_viewport(int width, int height)
    {
        viewport_.x = 0;
        viewport_.y = 0;
        viewport_.width = static_cast<float>(width);
        viewport_.height = static_cast<float>(height);
    }

private:
    struct FontHandle
    {
        int                    size = 16;
        litehtml::font_metrics metrics {};
    };

    BrowserApp                &app_;
    litehtml::position         viewport_;
    std::vector<litehtml::position> clip_stack_;

    litehtml::position current_clip() const;
    void fill_rect(HDLE hdc, const litehtml::position &pos, std::uint32_t color) const;
    void stroke_border(HDLE hdc,
                       float x1,
                       float y1,
                       float x2,
                       float y2,
                       float width,
                       std::uint32_t color) const;
    void blit_image(HDLE hdc,
                    const BrowserDecodedImage &image,
                    const litehtml::position &dest,
                    const litehtml::position &clip) const;
};

class BrowserApp
{
public:
    BrowserApp() : container_(*this)
    {
        std::snprintf(url_input_, sizeof(url_input_), "%s", "http://neverssl.com/");
        input_len_ = static_cast<int>(std::strlen(url_input_));
        std::snprintf(status_, sizeof(status_), "%s", "Ready");
    }

    int run();
    void handle_message(unsigned long long type, unsigned long long hData, unsigned long long lData);
    void request_navigation(const std::string &url);
    void set_title(const std::string &title);
    void set_base_url(const std::string &url) { base_url_ = url; }
    const std::string &base_url() const { return base_url_; }
    std::string resolve_url(const std::string &url, const std::string &base_url) const;
    BrowserFetchResult fetch_resource(const std::string &url);
    const BrowserDecodedImage *load_image_resource(const std::string &url);
    void invalidate_view() { need_redraw_ = true; }

private:
    struct CachedResource
    {
        std::string        url;
        BrowserFetchResult result;
    };

    struct CachedImage
    {
        std::string         url;
        BrowserDecodedImage image;
        bool                failed = false;
    };

    HDLE                   window_ = 0;
    BrowserContainer       container_;
    litehtml::document::ptr document_;
    std::string            base_url_;
    std::string            pending_url_;
    std::string            page_title_;
    bool                   need_redraw_ = true;
    bool                   load_pending_ = false;
    bool                   startup_guard_ = true;
    int                    scroll_y_ = 0;
    int                    text_content_height_ = 0;
    char                   url_input_[512] {};
    int                    input_len_ = 0;
    char                   status_[128] {};
    std::vector<std::string> text_lines_;
    std::vector<CachedResource> resource_cache_;
    std::vector<CachedImage> image_cache_;

    void load_current_url();
    void render();
    void set_status(const std::string &text);
    void prepare_text_view(const BrowserFetchResult &response);
    BrowserPreparedDocument prepare_document_html(const BrowserFetchResult &response);
    std::string run_script_compat(const std::string &script_source,
                                  const std::string &script_base_url,
                                  BrowserPreparedDocument &prepared);
    const BrowserFetchResult *find_cached_resource(const std::string &url) const;
    void store_cached_resource(const std::string &url, const BrowserFetchResult &result);
    const CachedImage *find_cached_image(const std::string &url) const;
    int  content_x() const { return BROWSER_MARGIN; }
    int  content_y() const { return BROWSER_HEADER_HEIGHT + BROWSER_MARGIN; }
    int  content_width() const { return BROWSER_WIDTH - BROWSER_MARGIN * 2; }
    int  content_height() const { return BROWSER_HEIGHT - content_y() - BROWSER_MARGIN; }
    void clamp_scroll();
    bool hit_content_area(int x, int y) const;
    void process_pointer_move(int x, int y);
    void process_pointer_click(int x, int y);
};

static BrowserApp *g_app = nullptr;

litehtml::position BrowserContainer::current_clip() const
{
    if (clip_stack_.empty()) {
        return viewport_;
    }
    return clip_stack_.back();
}

void BrowserContainer::fill_rect(HDLE hdc, const litehtml::position &pos, std::uint32_t color) const
{
    litehtml::position clipped = pos.intersect(current_clip());
    if (clipped.width <= 0 || clipped.height <= 0) {
        return;
    }

    xapi_DrawRect(hdc,
                  static_cast<std::uint32_t>(clipped.x),
                  static_cast<std::uint32_t>(clipped.y),
                  static_cast<std::uint32_t>(clipped.right()),
                  static_cast<std::uint32_t>(clipped.bottom()),
                  color,
                  true);
}

void BrowserContainer::stroke_border(HDLE hdc,
                                     float x1,
                                     float y1,
                                     float x2,
                                     float y2,
                                     float width,
                                     std::uint32_t color) const
{
    if (width <= 0.0f || x2 <= x1 || y2 <= y1) {
        return;
    }

    litehtml::position rect(x1, y1, x2 - x1, y2 - y1);
    fill_rect(hdc, rect, color);
}

litehtml::uint_ptr BrowserContainer::create_font(const litehtml::font_description &descr,
                                                 const litehtml::document *,
                                                 litehtml::font_metrics *fm)
{
    FontHandle *font = new FontHandle();
    font->size = std::max(10, static_cast<int>(descr.size));
    font->metrics.font_size = static_cast<float>(font->size);
    font->metrics.height = static_cast<float>(font->size + 4);
    font->metrics.ascent = static_cast<float>(font->size);
    font->metrics.descent = 4.0f;
    font->metrics.x_height = static_cast<float>(font->size * 2 / 3);
    font->metrics.ch_width = static_cast<float>(xapi_CalcTextWidth(const_cast<char *>("0"), static_cast<std::uint32_t>(font->size)));
    font->metrics.draw_spaces = true;
    font->metrics.sub_shift = 2.0f;
    font->metrics.super_shift = 2.0f;

    if (fm != nullptr) {
        *fm = font->metrics;
    }
    return reinterpret_cast<litehtml::uint_ptr>(font);
}

void BrowserContainer::delete_font(litehtml::uint_ptr hFont)
{
    delete reinterpret_cast<FontHandle *>(hFont);
}

litehtml::pixel_t BrowserContainer::text_width(const char *text, litehtml::uint_ptr hFont)
{
    FontHandle *font = reinterpret_cast<FontHandle *>(hFont);
    if (text == nullptr || font == nullptr) {
        return 0;
    }
    return static_cast<float>(xapi_CalcTextWidth(const_cast<char *>(text), static_cast<std::uint32_t>(font->size)));
}

void BrowserContainer::draw_text(litehtml::uint_ptr hdc,
                                 const char *text,
                                 litehtml::uint_ptr hFont,
                                 litehtml::web_color color,
                                 const litehtml::position &pos)
{
    FontHandle *font = reinterpret_cast<FontHandle *>(hFont);
    if (text == nullptr || font == nullptr) {
        return;
    }

    litehtml::position clip = current_clip();
    if (!clip.does_intersect(&pos)) {
        return;
    }

    xapi_DrawSWText(static_cast<HDLE>(hdc),
                    static_cast<std::uint32_t>(pos.x),
                    static_cast<std::uint32_t>(pos.y),
                    const_cast<char *>(text),
                    litehtml_color(color));
}

void BrowserContainer::draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker &marker)
{
    litehtml::position pos(marker.pos.x, marker.pos.y, 10.0f, 10.0f);
    fill_rect(static_cast<HDLE>(hdc), pos, litehtml_color(marker.color));
}

void BrowserContainer::load_image(const char *src, const char *baseurl, bool redraw_on_ready)
{
    const std::string resolved = app_.resolve_url(src == nullptr ? std::string() : std::string(src),
                                                  baseurl == nullptr ? std::string() : std::string(baseurl));
    if (resolved.empty()) {
        return;
    }

    if (app_.load_image_resource(resolved) != nullptr && redraw_on_ready) {
        app_.invalidate_view();
    }
}

void BrowserContainer::get_image_size(const char *src, const char *baseurl, litehtml::size &sz)
{
    sz.width = 0;
    sz.height = 0;

    const std::string resolved = app_.resolve_url(src == nullptr ? std::string() : std::string(src),
                                                  baseurl == nullptr ? std::string() : std::string(baseurl));
    if (resolved.empty()) {
        return;
    }

    const BrowserDecodedImage *image = app_.load_image_resource(resolved);
    if (image == nullptr || image->empty()) {
        return;
    }

    sz.width = static_cast<float>(image->width);
    sz.height = static_cast<float>(image->height);
}

void BrowserContainer::draw_image(litehtml::uint_ptr hdc,
                                  const litehtml::background_layer &layer,
                                  const std::string &url,
                                  const std::string &base_url)
{
    const std::string resolved = app_.resolve_url(url, base_url);
    if (resolved.empty()) {
        return;
    }

    const BrowserDecodedImage *image = app_.load_image_resource(resolved);
    if (image == nullptr || image->empty()) {
        return;
    }

    litehtml::position dest = layer.origin_box;
    if (dest.width <= 0 || dest.height <= 0) {
        dest = layer.border_box;
    }

    litehtml::position clip = layer.clip_box.intersect(current_clip());
    blit_image(static_cast<HDLE>(hdc), *image, dest, clip);
}

void BrowserContainer::blit_image(HDLE hdc,
                                  const BrowserDecodedImage &image,
                                  const litehtml::position &dest,
                                  const litehtml::position &clip) const
{
    if (image.empty() || dest.width <= 0 || dest.height <= 0 || clip.width <= 0 || clip.height <= 0) {
        return;
    }

    const int left = std::max(static_cast<int>(std::floor(dest.x)), static_cast<int>(std::floor(clip.x)));
    const int top = std::max(static_cast<int>(std::floor(dest.y)), static_cast<int>(std::floor(clip.y)));
    const int right = std::min(static_cast<int>(std::ceil(dest.right())), static_cast<int>(std::ceil(clip.right())));
    const int bottom = std::min(static_cast<int>(std::ceil(dest.bottom())), static_cast<int>(std::ceil(clip.bottom())));
    if (right <= left || bottom <= top) {
        return;
    }

    const std::size_t draw_width = static_cast<std::size_t>(right - left);
    const std::size_t draw_height = static_cast<std::size_t>(bottom - top);
    std::vector<XCOLORA> buffer(draw_width * draw_height);

    for (std::size_t y = 0; y < draw_height; ++y) {
        const float sample_y = ((static_cast<float>(top) + static_cast<float>(y) + 0.5f) - dest.y) / dest.height;
        int src_y = static_cast<int>(sample_y * static_cast<float>(image.height));
        src_y = std::clamp(src_y, 0, image.height - 1);

        for (std::size_t x = 0; x < draw_width; ++x) {
            const float sample_x = ((static_cast<float>(left) + static_cast<float>(x) + 0.5f) - dest.x) / dest.width;
            int src_x = static_cast<int>(sample_x * static_cast<float>(image.width));
            src_x = std::clamp(src_x, 0, image.width - 1);
            buffer[y * draw_width + x] = image.pixels[static_cast<std::size_t>(src_y) * static_cast<std::size_t>(image.width) +
                                                     static_cast<std::size_t>(src_x)];
        }
    }

    xapi_WriteBufferA(hdc,
                      static_cast<unsigned int>(left),
                      static_cast<unsigned int>(top),
                      static_cast<unsigned int>(draw_width),
                      static_cast<unsigned int>(draw_height),
                      buffer.data());
}

void BrowserContainer::draw_solid_fill(litehtml::uint_ptr hdc,
                                       const litehtml::background_layer &layer,
                                       const litehtml::web_color &color)
{
    fill_rect(static_cast<HDLE>(hdc), layer.border_box, litehtml_color(color));
}

void BrowserContainer::draw_linear_gradient(litehtml::uint_ptr hdc,
                                            const litehtml::background_layer &layer,
                                            const litehtml::background_layer::linear_gradient &gradient)
{
    if (!gradient.color_points.empty()) {
        draw_solid_fill(hdc, layer, gradient.color_points.front().color);
    }
}

void BrowserContainer::draw_radial_gradient(litehtml::uint_ptr hdc,
                                            const litehtml::background_layer &layer,
                                            const litehtml::background_layer::radial_gradient &gradient)
{
    if (!gradient.color_points.empty()) {
        draw_solid_fill(hdc, layer, gradient.color_points.front().color);
    }
}

void BrowserContainer::draw_conic_gradient(litehtml::uint_ptr hdc,
                                           const litehtml::background_layer &layer,
                                           const litehtml::background_layer::conic_gradient &gradient)
{
    if (!gradient.color_points.empty()) {
        draw_solid_fill(hdc, layer, gradient.color_points.front().color);
    }
}

void BrowserContainer::draw_borders(litehtml::uint_ptr hdc,
                                    const litehtml::borders &borders,
                                    const litehtml::position &draw_pos,
                                    bool)
{
    const HDLE handle = static_cast<HDLE>(hdc);
    stroke_border(handle, draw_pos.x, draw_pos.y, draw_pos.right(), draw_pos.y + borders.top.width, borders.top.width, litehtml_color(borders.top.color));
    stroke_border(handle, draw_pos.x, draw_pos.bottom() - borders.bottom.width, draw_pos.right(), draw_pos.bottom(), borders.bottom.width, litehtml_color(borders.bottom.color));
    stroke_border(handle, draw_pos.x, draw_pos.y, draw_pos.x + borders.left.width, draw_pos.bottom(), borders.left.width, litehtml_color(borders.left.color));
    stroke_border(handle, draw_pos.right() - borders.right.width, draw_pos.y, draw_pos.right(), draw_pos.bottom(), borders.right.width, litehtml_color(borders.right.color));
}

void BrowserContainer::set_caption(const char *caption)
{
    app_.set_title(caption == nullptr ? std::string() : std::string(caption));
}

void BrowserContainer::set_base_url(const char *base_url)
{
    app_.set_base_url(base_url == nullptr ? std::string() : std::string(base_url));
}

void BrowserContainer::on_anchor_click(const char *url, const litehtml::element::ptr &)
{
    if (url == nullptr || *url == '\0') {
        return;
    }

    litehtml::url resolved = litehtml::resolve(litehtml::url(app_.base_url()), litehtml::url(url));
    app_.request_navigation(resolved.str());
}

void BrowserContainer::import_css(litehtml::string &text, const litehtml::string &url, litehtml::string &baseurl)
{
    text.clear();

    if (url.empty()) {
        return;
    }

    std::string resolve_base = baseurl.empty() ? app_.base_url() : baseurl;
    if (resolve_base.empty()) {
        resolve_base = app_.base_url();
    }

    litehtml::url resolved = resolve_base.empty() ? litehtml::url(url)
                                                  : litehtml::resolve(litehtml::url(resolve_base), litehtml::url(url));

    BrowserFetchResult response = app_.fetch_resource(resolved.str());
    if (!response.ok) {
        log_line("css load failed: " + resolved.str() + " error=" + response.error);
        return;
    }

    text = response.body;
    baseurl = response.final_url.empty() ? resolved.str() : response.final_url;
}

void BrowserContainer::transform_text(litehtml::string &text, litehtml::text_transform tt)
{
    switch (tt) {
    case litehtml::text_transform_uppercase:
        for (char &ch : text) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        break;
    case litehtml::text_transform_lowercase:
        for (char &ch : text) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        break;
    case litehtml::text_transform_capitalize: {
        bool capitalize = true;
        for (char &ch : text) {
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                capitalize = true;
            } else if (capitalize) {
                ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                capitalize = false;
            }
        }
        break;
    }
    default:
        break;
    }
}

void BrowserContainer::set_clip(const litehtml::position &pos, const litehtml::border_radiuses &)
{
    clip_stack_.push_back(pos.intersect(current_clip()));
}

void BrowserContainer::del_clip()
{
    if (!clip_stack_.empty()) {
        clip_stack_.pop_back();
    }
}

void BrowserContainer::get_media_features(litehtml::media_features &media) const
{
    media.type = litehtml::media_type_screen;
    media.width = viewport_.width;
    media.height = viewport_.height;
    media.device_width = viewport_.width;
    media.device_height = viewport_.height;
    media.color = 8;
    media.color_index = 256;
    media.monochrome = 0;
    media.resolution = 96.0f;
}

void BrowserContainer::get_language(litehtml::string &language, litehtml::string &culture) const
{
    language = "en";
    culture = "US";
}

void BrowserApp::set_title(const std::string &title)
{
    page_title_ = title;
    need_redraw_ = true;
}

void BrowserApp::set_status(const std::string &text)
{
    std::snprintf(status_, sizeof(status_), "%s", text.c_str());
    need_redraw_ = true;
}

std::string BrowserApp::run_script_compat(const std::string &script_source,
                                          const std::string &script_base_url,
                                          BrowserPreparedDocument &prepared)
{
    std::string injected_html;

    std::size_t search_pos = 0;
    while (search_pos < script_source.size()) {
        std::size_t write_call = std::string::npos;
        std::size_t write_arg = 0;
        std::size_t writeln_call = std::string::npos;
        std::size_t writeln_arg = 0;
        const bool has_write = find_js_call(script_source, "document.write", search_pos, write_call, write_arg);
        const bool has_writeln = find_js_call(script_source, "document.writeln", search_pos, writeln_call, writeln_arg);
        if (!has_write && !has_writeln) {
            break;
        }

        const bool use_writeln = !has_write || (has_writeln && writeln_call < write_call);
        const std::size_t call_pos = use_writeln ? writeln_call : write_call;
        const std::size_t arg_pos = use_writeln ? writeln_arg : write_arg;

        std::string value;
        std::size_t after_value = 0;
        if (parse_js_string_expression(script_source, arg_pos, value, after_value)) {
            after_value = skip_js_space(script_source, after_value);
            if (after_value < script_source.size() && script_source[after_value] == ')') {
                injected_html += value;
                if (use_writeln) {
                    injected_html.push_back('\n');
                }
                ++prepared.document_writes;
            }
        }

        search_pos = call_pos + 1;
    }

    if (prepared.redirect_url.empty()) {
        std::string redirect_value;
        std::size_t match_pos = std::string::npos;
        std::size_t value_pos = 0;
        std::size_t after_value = 0;

        const auto try_redirect_call = [&](const char *needle) -> bool {
            if (!find_js_call(script_source, needle, 0, match_pos, value_pos)) {
                return false;
            }
            if (!parse_js_string_expression(script_source, value_pos, redirect_value, after_value)) {
                return false;
            }
            after_value = skip_js_space(script_source, after_value);
            return after_value < script_source.size() && script_source[after_value] == ')';
        };

        const auto try_redirect_assignment = [&](const char *needle) -> bool {
            if (!find_js_assignment(script_source, needle, 0, match_pos, value_pos)) {
                return false;
            }
            return parse_js_string_expression(script_source, value_pos, redirect_value, after_value);
        };

        if (try_redirect_call("window.location.replace") ||
            try_redirect_call("location.replace") ||
            try_redirect_call("window.location.assign") ||
            try_redirect_call("location.assign") ||
            try_redirect_assignment("window.location.href") ||
            try_redirect_assignment("location.href") ||
            try_redirect_assignment("window.location")) {
            prepared.redirect_url = resolve_url(redirect_value, script_base_url);
        }
    }

    if (prepared.title_override.empty()) {
        std::string title_value;
        std::size_t match_pos = std::string::npos;
        std::size_t value_pos = 0;
        std::size_t after_value = 0;
        if (find_js_assignment(script_source, "document.title", 0, match_pos, value_pos) &&
            parse_js_string_expression(script_source, value_pos, title_value, after_value)) {
            prepared.title_override = title_value;
        }
    }

    return injected_html;
}

BrowserPreparedDocument BrowserApp::prepare_document_html(const BrowserFetchResult &response)
{
    BrowserPreparedDocument prepared;

    if (!looks_like_html(response)) {
        prepared.fallback_used = true;
        return prepared;
    }

    std::string html = response.body;
    erase_tag_markers(html, "noscript");

    std::string output;
    output.reserve(html.size());

    std::size_t cursor = 0;
    while (cursor < html.size()) {
        const std::size_t script_pos = find_case_insensitive(html, "<script", cursor);
        if (script_pos == std::string::npos) {
            output.append(html, cursor, html.size() - cursor);
            break;
        }

        output.append(html, cursor, script_pos - cursor);

        const std::size_t open_end = html.find('>', script_pos);
        if (open_end == std::string::npos) {
            break;
        }
        const std::size_t close_pos = find_case_insensitive(html, "</script", open_end + 1);
        if (close_pos == std::string::npos) {
            break;
        }
        const std::size_t close_end = html.find('>', close_pos);
        if (close_end == std::string::npos) {
            break;
        }

        ++prepared.script_blocks;

        const std::string open_tag = html.substr(script_pos, open_end - script_pos + 1);
        const std::string type = extract_tag_attribute(open_tag, "type");
        if (is_executable_script_type(type)) {
            std::string script_base_url = base_url_;
            std::string script_source = html.substr(open_end + 1, close_pos - open_end - 1);

            const std::string src = extract_tag_attribute(open_tag, "src");
            if (!src.empty()) {
                const std::string resolved = resolve_url(src, base_url_);
                if (!resolved.empty()) {
                    BrowserFetchResult script_response = fetch_resource(resolved);
                    if (script_response.ok) {
                        script_source = script_response.body;
                        script_base_url = script_response.final_url.empty() ? resolved : script_response.final_url;
                        ++prepared.external_scripts;
                    } else {
                        log_line("script load failed: " + shorten_url_for_log(resolved) + " error=" + script_response.error);
                        script_source.clear();
                    }
                }
            }

            const int writes_before = prepared.document_writes;
            const bool had_redirect = !prepared.redirect_url.empty();
            const bool had_title = !prepared.title_override.empty();
            std::string injected = run_script_compat(script_source, script_base_url, prepared);
            if (!injected.empty() ||
                prepared.document_writes != writes_before ||
                (!had_redirect && !prepared.redirect_url.empty()) ||
                (!had_title && !prepared.title_override.empty())) {
                ++prepared.scripts_with_effect;
            }
            output += injected;
        }

        cursor = close_end + 1;
    }

    html.swap(output);

    constexpr std::size_t kMaxHtmlBytes = 256 * 1024;
    if (html.size() > kMaxHtmlBytes) {
        prepared.fallback_used = true;
        return prepared;
    }

    if (!prepared.title_override.empty()) {
        set_html_title(html, prepared.title_override);
    }

    if (find_case_insensitive(html, "<html") == std::string::npos &&
        find_case_insensitive(html, "<body") == std::string::npos) {
        html = build_fallback_document("HTML Fragment",
                                       "The response does not contain a full HTML document. Rendering a simplified wrapper.",
                                       html);
    }

    prepared.html = std::move(html);
    return prepared;
}

std::string BrowserApp::resolve_url(const std::string &url, const std::string &base_url) const
{
    const std::string trimmed = trim_copy(url);
    if (trimmed.empty()) {
        return std::string();
    }
    if (lower_copy(trimmed).rfind("data:", 0) == 0) {
        return trimmed;
    }

    std::string resolve_base = trim_copy(base_url);
    if (resolve_base.empty()) {
        resolve_base = base_url_.empty() ? std::string() : base_url_;
    }

    litehtml::url resolved = resolve_base.empty() ? litehtml::url(trimmed)
                                                  : litehtml::resolve(litehtml::url(resolve_base), litehtml::url(trimmed));
    const std::string final_url = resolved.str();
    const std::string scheme = lower_copy(final_url.substr(0, final_url.find(':')));
    if (scheme != "http" && scheme != "https") {
        return std::string();
    }
    return final_url;
}

const BrowserFetchResult *BrowserApp::find_cached_resource(const std::string &url) const
{
    for (const CachedResource &entry : resource_cache_) {
        if (entry.url == url) {
            return &entry.result;
        }
    }
    return nullptr;
}

void BrowserApp::store_cached_resource(const std::string &url, const BrowserFetchResult &result)
{
    for (CachedResource &entry : resource_cache_) {
        if (entry.url == url) {
            entry.result = result;
            return;
        }
    }

    constexpr std::size_t kMaxCachedResources = 24;
    if (resource_cache_.size() >= kMaxCachedResources) {
        resource_cache_.erase(resource_cache_.begin());
    }

    CachedResource entry;
    entry.url = url;
    entry.result = result;
    resource_cache_.push_back(std::move(entry));
}

const BrowserApp::CachedImage *BrowserApp::find_cached_image(const std::string &url) const
{
    for (const CachedImage &entry : image_cache_) {
        if (entry.url == url) {
            return &entry;
        }
    }
    return nullptr;
}

BrowserFetchResult BrowserApp::fetch_resource(const std::string &url)
{
    if (const BrowserFetchResult *cached = find_cached_resource(url)) {
        return *cached;
    }

    BrowserFetchResult inline_result;
    if (decode_data_url(url, inline_result)) {
        if (inline_result.ok) {
            store_cached_resource(url, inline_result);
        }
        return inline_result;
    }

    BrowserFetchResult result = browser_fetch_url(url);
    if (result.ok) {
        const std::string cache_key = result.final_url.empty() ? url : result.final_url;
        store_cached_resource(cache_key, result);
        if (cache_key != url) {
            store_cached_resource(url, result);
        }
    }
    return result;
}

const BrowserDecodedImage *BrowserApp::load_image_resource(const std::string &url)
{
    if (url.empty()) {
        return nullptr;
    }

    if (const CachedImage *cached = find_cached_image(url)) {
        return cached->failed ? nullptr : &cached->image;
    }

    constexpr std::size_t kMaxCachedImages = 16;
    if (image_cache_.size() >= kMaxCachedImages) {
        image_cache_.erase(image_cache_.begin());
    }

    CachedImage entry;
    entry.url = url;

    BrowserFetchResult response = fetch_resource(url);
    if (!response.ok) {
        entry.failed = true;
        image_cache_.push_back(std::move(entry));
        log_line("image load failed: " + shorten_url_for_log(url) + " error=" + response.error +
                 " ctype=" + (response.content_type.empty() ? std::string("-") : response.content_type));
        return nullptr;
    }

    std::string decode_error;
    if (!browser_decode_image(response.body, response.content_type, entry.image, decode_error)) {
        entry.failed = true;
        image_cache_.push_back(std::move(entry));
        log_line("image decode failed: " + shorten_url_for_log(url) + " error=" + decode_error +
                 " ctype=" + (response.content_type.empty() ? std::string("-") : response.content_type));
        return nullptr;
    }

    char info[256];
    std::snprintf(info,
                  sizeof(info),
                  "image ready: %s %dx%d ctype=%s",
                  shorten_url_for_log(url).c_str(),
                  entry.image.width,
                  entry.image.height,
                  response.content_type.empty() ? "-" : response.content_type.c_str());
    log_line(info);

    image_cache_.push_back(std::move(entry));
    return &image_cache_.back().image;
}

void BrowserApp::request_navigation(const std::string &url)
{
    if (url.empty()) {
        return;
    }

    pending_url_ = url;
    std::snprintf(url_input_, sizeof(url_input_), "%s", url.c_str());
    input_len_ = static_cast<int>(std::strlen(url_input_));
    load_pending_ = true;
    need_redraw_ = true;
}

void BrowserApp::clamp_scroll()
{
    if (!document_) {
        const int max_scroll = std::max(0, text_content_height_ - content_height());
        scroll_y_ = std::clamp(scroll_y_, 0, max_scroll);
        return;
    }

    const int max_scroll = std::max(0, static_cast<int>(document_->height()) - content_height());
    scroll_y_ = std::clamp(scroll_y_, 0, max_scroll);
}

void BrowserApp::prepare_text_view(const BrowserFetchResult &response)
{
    std::string text = looks_like_html(response) ? html_to_text(response.body) : response.body;
    text = trim_copy(text);
    if (text.empty()) {
        text = "Page loaded, but no visible text content was produced.";
    }

    text_lines_ = wrap_text_for_width(text, std::max(120, content_width() - 24));
    text_content_height_ = static_cast<int>(text_lines_.size()) * BROWSER_TEXT_LINE_HEIGHT + 24;
}

bool BrowserApp::hit_content_area(int x, int y) const
{
    return x >= content_x() && x < content_x() + content_width() &&
           y >= content_y() && y < content_y() + content_height();
}

void BrowserApp::process_pointer_move(int x, int y)
{
    if (!document_ || !hit_content_area(x, y)) {
        return;
    }

    litehtml::position::vector redraw;
    const int client_x = x - content_x();
    const int client_y = y - content_y();
    document_->on_mouse_over(static_cast<float>(client_x),
                             static_cast<float>(client_y + scroll_y_),
                             static_cast<float>(client_x),
                             static_cast<float>(client_y),
                             redraw);
    need_redraw_ = true;
}

void BrowserApp::process_pointer_click(int x, int y)
{
    if (!document_ || !hit_content_area(x, y)) {
        return;
    }

    litehtml::position::vector redraw;
    const int client_x = x - content_x();
    const int client_y = y - content_y();
    document_->on_lbutton_down(static_cast<float>(client_x),
                               static_cast<float>(client_y + scroll_y_),
                               static_cast<float>(client_x),
                               static_cast<float>(client_y),
                               redraw);
    document_->on_lbutton_up(static_cast<float>(client_x),
                             static_cast<float>(client_y + scroll_y_),
                             static_cast<float>(client_x),
                             static_cast<float>(client_y),
                             redraw);
    need_redraw_ = true;
}

void BrowserApp::handle_message(unsigned long long type, unsigned long long hData, unsigned long long lData)
{
    switch (type) {
    case MSG_CHAR:
        if (static_cast<char>(lData) >= ' ' && input_len_ < static_cast<int>(sizeof(url_input_) - 1)) {
            startup_guard_ = false;
            url_input_[input_len_++] = static_cast<char>(lData);
            url_input_[input_len_] = '\0';
            need_redraw_ = true;
        }
        break;
    case MSG_SPCHAR:
        if (static_cast<char>(lData) == '\b') {
            if (input_len_ > 0) {
                startup_guard_ = false;
                url_input_[--input_len_] = '\0';
                need_redraw_ = true;
            }
        } else if (static_cast<char>(lData) == '\n') {
            if (!startup_guard_) {
                pending_url_ = url_input_;
                load_pending_ = true;
            } else {
                startup_guard_ = false;
            }
        }
        break;
    case MSG_MOVE:
        process_pointer_move(static_cast<int>(hData), static_cast<int>(lData));
        break;
    case MSG_LBUTTON:
        process_pointer_click(static_cast<int>(hData), static_cast<int>(lData));
        break;
    case MSG_ROLLER:
        scroll_y_ -= static_cast<int>(hData) * BROWSER_SCROLL_STEP;
        clamp_scroll();
        need_redraw_ = true;
        break;
    default:
        break;
    }
}

void BrowserApp::load_current_url()
{
    load_pending_ = false;
    set_status("Loading...");
    render();

    std::string target = pending_url_.empty() ? std::string(url_input_) : pending_url_;
    try {
        BrowserFetchResult response;
        BrowserPreparedDocument prepared;

        constexpr int kMaxCompatRedirects = 3;
        int compat_redirects = 0;
        while (true) {
            log_line("fetch begin url=" + target);
            response = fetch_resource(target);
            if (!response.ok) {
                log_line("load failed: " + response.error);
                document_.reset();
                base_url_ = target;
                scroll_y_ = 0;
                set_status("Load failed");
                return;
            }

            base_url_ = response.final_url.empty() ? target : response.final_url;

            char info[256];
            std::snprintf(info,
                          sizeof(info),
                          "fetch ok status=%d bytes=%llu ctype=%s charset=%s",
                          response.status_code,
                          static_cast<unsigned long long>(response.body.size()),
                          response.content_type.empty() ? "-" : response.content_type.c_str(),
                          response.charset.empty() ? "-" : response.charset.c_str());
            log_line(info);

            prepared = BrowserPreparedDocument();
            if (BROWSER_ENABLE_LITEHTML) {
                prepared = prepare_document_html(response);
                if (prepared.script_blocks > 0) {
                    char js_info[256];
                    std::snprintf(js_info,
                                  sizeof(js_info),
                                  "js compat scripts=%d external=%d writes=%d effects=%d",
                                  prepared.script_blocks,
                                  prepared.external_scripts,
                                  prepared.document_writes,
                                  prepared.scripts_with_effect);
                    log_line(js_info);
                }
            }

            if (!prepared.redirect_url.empty() &&
                prepared.redirect_url != target &&
                compat_redirects < kMaxCompatRedirects) {
                log_line("js redirect: " + shorten_url_for_log(target) + " -> " + shorten_url_for_log(prepared.redirect_url));
                target = prepared.redirect_url;
                ++compat_redirects;
                continue;
            }
            break;
        }

        page_title_.clear();
        scroll_y_ = 0;
        document_.reset();
        text_lines_.clear();
        text_content_height_ = 0;

        bool prepared_fallback = !BROWSER_ENABLE_LITEHTML || prepared.fallback_used;
        if (!prepared_fallback) {
            litehtml::estring html(prepared.html);
            html.encoding = litehtml::encoding::utf_8;
            html.confidence = litehtml::confidence::certain;
            if (!response.charset.empty()) {
                litehtml::encoding declared_encoding = litehtml::get_encoding(lower_copy(response.charset));
                if (declared_encoding != litehtml::encoding::null) {
                    html.encoding = declared_encoding;
                }
            }

            container_.set_viewport(content_width(), content_height());
            log_line("document create begin");
            document_ = litehtml::document::createFromString(html, &container_);
            if (document_) {
                log_line("document render begin");
                document_->render(static_cast<float>(content_width()));
                log_line("document render done");
            } else {
                prepared_fallback = true;
            }
        }

        if (prepared_fallback) {
            if (!BROWSER_ENABLE_LITEHTML) {
                log_line("litehtml disabled: safe mode active");
            }
            prepare_text_view(response);
            log_line("safe text render ready");
        }
        clamp_scroll();
        set_status("Loaded");
        need_redraw_ = true;
    } catch (const std::exception &ex) {
        log_line(std::string("exception: ") + ex.what());
        document_.reset();
        set_status("Exception");
    } catch (...) {
        log_line("unknown exception");
        document_.reset();
        set_status("Exception");
    }
}

void BrowserApp::render()
{
    xapi_DrawRect(window_, 0, 0, BROWSER_WIDTH - 1, BROWSER_HEIGHT - 1, COLOR_BG, true);
    xapi_DrawRect(window_, 0, 0, BROWSER_WIDTH - 1, BROWSER_HEADER_HEIGHT - 1, COLOR_HEADER, true);
    xapi_DrawRect(window_, BROWSER_MARGIN, BROWSER_HEADER_HEIGHT + 2, BROWSER_WIDTH - BROWSER_MARGIN, BROWSER_HEIGHT - BROWSER_MARGIN, COLOR_PANEL, true);
    xapi_DrawRect(window_,
                  BROWSER_INPUT_X,
                  BROWSER_INPUT_Y,
                  BROWSER_WIDTH - BROWSER_MARGIN,
                  BROWSER_INPUT_Y + BROWSER_INPUT_HEIGHT,
                  COLOR_INPUT,
                  true);

    xapi_DrawSWText(window_, 18, 18, const_cast<char *>("Browser"), COLOR_MUTED);
    xapi_DrawSWText(window_, 20, 48, const_cast<char *>(status_), COLOR_STATUS);
    xapi_DrawSWText(window_, 20, 18, const_cast<char *>("URL"), COLOR_MUTED);
    xapi_DrawSWText(window_, BROWSER_INPUT_X + 8, BROWSER_INPUT_Y + 6, url_input_, COLOR_TEXT);

    const std::string title = page_title_.empty() ? std::string("litehtml browser") : page_title_;
    xapi_DrawSWText(window_, BROWSER_WIDTH - 360, 18, const_cast<char *>(title.c_str()), COLOR_MUTED);

    xapi_DrawRect(window_,
                  content_x(),
                  content_y(),
                  content_x() + content_width(),
                  content_y() + content_height(),
                  COLOR_PANEL,
                  true);

    if (document_) {
        try {
            container_.set_viewport(content_width(), content_height());
            litehtml::position clip(static_cast<float>(content_x()),
                                    static_cast<float>(content_y()),
                                    static_cast<float>(content_width()),
                                    static_cast<float>(content_height()));
            document_->draw(window_, static_cast<float>(content_x()), static_cast<float>(content_y() - scroll_y_), &clip);
        } catch (const std::exception &ex) {
            log_line(std::string("draw exception: ") + ex.what());
            document_.reset();
            set_status("Draw exception");
        } catch (...) {
            log_line("draw unknown exception");
            document_.reset();
            set_status("Draw exception");
        }
    } else if (!text_lines_.empty()) {
        const int start_line = std::max(0, scroll_y_ / BROWSER_TEXT_LINE_HEIGHT);
        const int visible_lines = content_height() / BROWSER_TEXT_LINE_HEIGHT + 2;
        for (int i = 0; i < visible_lines; ++i) {
            const int line_index = start_line + i;
            if (line_index < 0 || line_index >= static_cast<int>(text_lines_.size())) {
                break;
            }
            const int y = content_y() + 12 + i * BROWSER_TEXT_LINE_HEIGHT - (scroll_y_ % BROWSER_TEXT_LINE_HEIGHT);
            if (y >= content_y() + content_height()) {
                break;
            }
            xapi_DrawSWText(window_,
                            content_x() + 12,
                            static_cast<unsigned int>(y),
                            const_cast<char *>(text_lines_[line_index].c_str()),
                            COLOR_TEXT);
        }
    } else {
        xapi_DrawSWText(window_, content_x() + 12, content_y() + 12, const_cast<char *>("No page loaded."), COLOR_ERROR);
    }

    xapi_RefreshWindow(window_);
    need_redraw_ = false;
}

int BrowserApp::run()
{
    g_app = this;

    XWINDOW window {};
    window.title = const_cast<char *>("Browser");
    window.width = BROWSER_WIDTH;
    window.height = BROWSER_HEIGHT;
    window.sets = XWIN_NORMAL;

    xapi_CreateWindow(&window_, &window);
    xapi_SetIcon(window_, const_cast<char *>("/system/icon/terminal.png"));
    SetMsgPrcor(window_, [](unsigned long long type, unsigned long long hData, unsigned long long lData) {
        if (g_app != nullptr) {
            g_app->handle_message(type, hData, lData);
        }
    });

    render();

    while (true) {
        if (load_pending_) {
            load_current_url();
        }
        if (need_redraw_) {
            render();
        }
        xapi_Sleep(BROWSER_LOOP_SLEEP_MS);
    }

    return 0;
}

} // namespace

static int browser_main_impl(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    BrowserApp app;
    return app.run();
}

extern "C" int browser_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int browser_main_cpp(int argc, char *argv[], char *envp[])
{
    return browser_main_impl(argc, argv, envp);
}
