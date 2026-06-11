#pragma once

#include <string>

/**
 * open the given url in the default browser using std::system()
 *
 * the URL must be vaild to be opened.
 * use open_url() to check if convert a string to a valid URL.
 *
 * @param url the URL to open.
 * @return void
 */
auto open_url(const std::string& url) -> void;

/**
 * @brief encode a string into URL encoding.
 *
 * check: https://www.w3schools.com/tags/ref_urlencode.ASP
 *
 * @param text the string to encode.
 * @return URL encoded string.
 */
auto encode_url(const std::string& text) -> std::string;
