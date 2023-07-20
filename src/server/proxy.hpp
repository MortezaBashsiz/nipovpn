#ifndef PROXY_HPP
#define PROXY_HPP

#include <string>

std::string encrypt(std::string content, int plainLen, std::string token);
std::string decrypt(std::string content, int plainLen, std::string token);

#endif // PROXY_HPP