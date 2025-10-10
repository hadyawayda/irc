#ifndef UTILS_HPP
#define UTILS_HPP

/**
 * @file Utils.hpp
 * @brief Small free functions for string processing and IRC parsing.
 */

#include <string>
#include <vector>

/**
 * @brief ASCII lower-case transformation (locale-independent).
 * @param s Input string
 * @return Lower-cased copy of s
 */
std::string toLower(const std::string& s);

/**
 * @brief Split an IRC line into command, parameters and trailing field.
 *
 * Implements the standard IRC tokenization where the last parameter can be a
 * trailing string after ':' that may include spaces.
 *
 * @param line     Raw line (without CRLF)
 * @param command  Output: upper-cased command token
 * @param params   Output: positional parameters (no leading ':')
 * @param trailing Output: trailing content (without leading ':'), empty if none
 */
void splitCmd(const std::string& line,
              std::string& command,
              std::vector<std::string>& params,
              std::string& trailing);

/** @return true if name looks like a channel identifier (e.g., starts with '#'). */
bool isChannelName(const std::string& name);
/** @return true if nick satisfies simplified RFC constraints for this project. */
bool isNickValid(const std::string& nick);

#endif
