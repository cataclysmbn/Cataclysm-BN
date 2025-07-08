#pragma once

#include <string>
#include <vector>

class translation;

/**
 * Perform case insensitive search for a query string inside a subject string.
 *
 * Searches for string given by qry inside a subject string given by str.
 *
 * @param str Subject to search for occurrence of the query string.
 * @param qry Query string to search for in str
 *
 * @return true if the query string is found at least once within the subject
 *         string, otherwise returns false
 */
bool lcmatch( const std::string &str, const std::string &qry );
bool lcmatch( const translation &str, const std::string &qry );

/** Perform case insensitive comparison of 2 strings. */
bool lcequal( const std::string &str1, const std::string &str2 );

/**
 * Matches text case insensitive with the include/exclude rules of the filter
 *
 * Multiple includes/excludes are possible
 *
 * Examle: bank,-house,tank,-car
 * Will match text containing tank or bank while not containing house or car
 *
 * @param text String to be matched
 * @param filter String with include/exclude rules
 *
 * @return true if include/exclude rules pass. See Example.
 */
bool match_include_exclude( const std::string &text, std::string filter );

/**
 * Joins a vector of `std::string`s into a single string with a delimiter/joiner
 */
std::string join( const std::vector<std::string> &strings, const std::string &joiner );

/**
 * Split string by delimiter
 */
std::vector<std::string> string_split( const std::string &text_in, char delim );

/**
 * Find position of str2 within str1 (case-insensitive)
 */
int ci_find_substr( const std::string &str1, const std::string &str2 );

/**
 * Match text containing wildcards (*)
 * @param text_in Text to check
 * @param pattern_in Pattern to check text_in against
 * Case insensitive search
 * Possible patterns:
 * *
 * wooD
 * wood*
 * *wood
 * Wood*aRrOW
 * wood*arrow*
 * *wood*arrow
 * *wood*hard* *x*y*z*arrow*
 */
bool wildcard_match( const std::string &text_in, const std::string &pattern_in );

/**
 * Remove excessive '*' in wildcard rule.
 */
std::string wildcard_trim_rule( const std::string &pattern_in );

/**
 * Remove spaces from the start and the end of a string.
 */
std::string trim( const std::string &s );

/**
 * Removes punctuation marks from the start and the end of a string.
 */
std::string trim_punctuation_marks( const std::string &s );

/**
 * Converts the string to upper case.
 */
std::string to_upper_case( const std::string &s );

/**
 * Converts the string to lower case.
 */
std::string to_lower_case( const std::string &s );

/**
 * Replace name tags with actual names.
 */
void replace_name_tags( std::string &input );

/**
 * Replace city tag with given name.
 */
void replace_city_tag( std::string &input, const std::string &name );

/**
 * Replace first occurence of 'what' within 'input' with 'with'.
 */
void replace_first( std::string &input, const std::string &what, const std::string &with );

/**
 * Replace all occurences of 'what' within 'input' with 'with'.
 */
std::string replace_all( std::string input, const std::string &what, const std::string &with );

/**
 * Replace special color tags (e.g. info, bold, bad) with actual color tags.
 */
std::string replace_colors( std::string text );

/**
 * Capitalize nth ASCII letter. Don't use for Unicode strings!
 */
std::string &capitalize_letter( std::string &str, size_t n = 0 );

/**
 * Remove leading and trailing whitespaces.
 */
std::string trim_whitespaces( const std::string &str );


