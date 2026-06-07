#pragma once

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace http_utils {

    inline std::string makeHostPort(const std::string &host, uint16_t port) {
        if (host.find(':') != std::string::npos &&
            !(host.size() >= 2 && host.front() == '[' && host.back() == ']')) {
            return "[" + host + "]:" + std::to_string(port);
        }

        return host + ":" + std::to_string(port);
    }

    inline std::string toLowerCopy(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        return value;
    }

    inline std::string trimCopy(std::string value) {
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.erase(value.begin());
        }

        while (!value.empty() &&
               (value.back() == ' ' ||
                value.back() == '\t' ||
                value.back() == '\r' ||
                value.back() == '\n')) {
            value.pop_back();
        }

        return value;
    }

    inline std::string extractHeaders(const std::string &message) {
        const auto pos = message.find("\r\n\r\n");
        if (pos == std::string::npos) {
            return {};
        }

        return message.substr(0, pos + 4);
    }

    inline std::string extractBody(const std::string &message) {
        const auto pos = message.find("\r\n\r\n");
        if (pos == std::string::npos) {
            return {};
        }

        return message.substr(pos + 4);
    }

    inline bool parseContentLength(const std::string &headers, std::size_t &value) {
        std::istringstream iss(headers);
        std::string line;

        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            const auto lower = toLowerCopy(line);
            if (lower.rfind("content-length:", 0) == 0) {
                auto raw = line.substr(std::string("Content-Length:").size());
                raw.erase(0, raw.find_first_not_of(" \t"));

                try {
                    value = static_cast<std::size_t>(std::stoull(raw));
                    return true;
                } catch (...) {
                    return false;
                }
            }
        }

        return false;
    }

    inline bool isChunked(const std::string &headers) {
        std::istringstream iss(headers);
        std::string line;

        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            const auto lower = toLowerCopy(line);
            if (lower.rfind("transfer-encoding:", 0) == 0 &&
                lower.find("chunked") != std::string::npos) {
                return true;
            }
        }

        return false;
    }

    inline std::string getRawHeader(const std::string &headers, const std::string &name) {
        std::istringstream iss(headers);
        std::string line;

        const std::string prefix = toLowerCopy(name) + ":";

        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            const std::string lower = toLowerCopy(line);
            if (lower.rfind(prefix, 0) == 0) {
                return trimCopy(line.substr(name.size() + 1));
            }
        }

        return {};
    }

}// namespace http_utils
