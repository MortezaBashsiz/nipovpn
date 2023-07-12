#include "net.hpp"

#include <string>

const std::string responseOk = "HTTP/1.0 200 OK\r\n";
const std::string responseBadRequest = "HTTP/1.0 400 Bad Request\r\n";
const std::string responseUnauthorized = "HTTP/1.0 401 Unauthorized\r\n";
const std::string responseForbidden = "HTTP/1.0 403 Forbidden\r\n";
const std::string responseNotFound = "HTTP/1.0 404 Not Found\r\n";
const std::string responseInternalServerError = "HTTP/1.0 500 Internal Server Error\r\n";
const std::string responseServiceUnavailable = "HTTP/1.0 503 Service Unavailable\r\n";

const char name_value_separator[] = { ':', ' ' };
const char crlf[] = { '\r', '\n' };