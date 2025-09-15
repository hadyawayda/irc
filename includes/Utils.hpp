#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

std::string toLower(const std::string& s);

void splitCmd(const std::string& line,
              std::string& command,
              std::vector<std::string>& params,
              std::string& trailing);

bool isChannelName(const std::string& name);
bool isNickValid(const std::string& nick);

#endif
