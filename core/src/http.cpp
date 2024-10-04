#include "http.hpp"

HTTP::HTTP(const std::shared_ptr<Config> &config,
           const std::shared_ptr<Log> &log, boost::asio::streambuf &buffer, boost::uuids::uuid uuid)
    : config_(config),
      log_(log),
      buffer_(buffer),
      parsedHttpRequest_(),
      httpType_(HTTP::HttpType::https),
      parsedTlsRequest_{"", "", TlsTypes::TLSHandshake},
      uuid_(uuid) {
    chunkHeader_ = "no";
}

HTTP::HTTP(const HTTP &http)
    : config_(http.config_),
      log_(http.log_),
      buffer_(http.buffer_),
      parsedHttpRequest_(http.parsedHttpRequest_),
      parsedTlsRequest_(http.parsedTlsRequest_),
      uuid_(http.uuid_) {
    chunkHeader_ = "no";
}

HTTP::~HTTP() {}

bool HTTP::detectType() {
    std::string requestStr(hexStreambufToStr(buffer_));
    std::string tmpStr;
    unsigned short pos = 0;
    tmpStr = requestStr.substr(pos, 2);
    if (tmpStr == "16" || tmpStr == "14" || tmpStr == "17") {
        httpType(HTTP::HttpType::https);
        parsedTlsRequest_.body = requestStr;
        if (parseTls())
            return true;
        else
            return false;
    } else {
        if (parseHttp()) {
            return true;
        } else
            return false;
    }
}

bool HTTP::parseHttp() {
    std::string requestStr(streambufToString(buffer_));
    boost::beast::http::request_parser<boost::beast::http::string_body> parser;
    parser.eager(true);
    boost::beast::error_code error;
    parser.put(boost::asio::buffer(requestStr), error);
    if (error) {
        log_->write(std::string("[" + to_string(uuid_) + "] [HTTP parseHttp] ") + error.what(),
                    Log::Level::DEBUG);
        return false;
    } else {
        parsedHttpRequest_ = parser.get();
        if (parsedHttpRequest_.method() == boost::beast::http::verb::connect)
            httpType(HTTP::HttpType::connect);
        else
            httpType(HTTP::HttpType::http);
        setIPPort();
        return true;
    }
}

bool HTTP::parseHttpResp() {
    std::string requestStr(streambufToString(buffer_));
    parser_.eager(true);
    boost::beast::error_code error;
    parser_.put(boost::asio::buffer(requestStr), error);
    if (error) {
        log_->write(std::string("[" + to_string(uuid_) + "] [HTTP parseHttpResp] ") + error.what(),
                    Log::Level::DEBUG);
        return false;
    } else {
        parsedHttpResponse_ = parser_.release();
        return true;
    }
}

bool HTTP::parseTls() {
    std::string tmpStr;
    unsigned short pos = 0;
    tmpStr = parsedTlsRequest_.body.substr(pos, 2);
    if (tmpStr == "16") {
        parsedTlsRequest_.type = TlsTypes::TLSHandshake;
        pos = 10;
        tmpStr = parsedTlsRequest_.body.substr(pos, 2);
        if (tmpStr == "01") {
            unsigned short tmpPos(0);
            pos = 86;
            tmpStr = parsedTlsRequest_.body.substr(pos, 2);
            tmpPos = hexToInt(tmpStr);
            pos += 2;
            tmpStr = parsedTlsRequest_.body.substr(pos, tmpPos);
            pos = pos + (tmpPos * 2);
            tmpStr = parsedTlsRequest_.body.substr(pos, 4);
            tmpPos = hexToInt(tmpStr);
            pos += 4;
            pos = pos + (tmpPos * 2);
            tmpStr = parsedTlsRequest_.body.substr(pos, 2);
            tmpPos = hexToInt(tmpStr);
            pos += 2;
            pos = pos + (tmpPos * 2);
            tmpStr = parsedTlsRequest_.body.substr(pos, 4);
            tmpPos = hexToInt(tmpStr);
            pos += 4;
            tmpStr = parsedTlsRequest_.body.substr(pos, 4);
            if (hexToInt(tmpStr) == 0) {
                pos += 14;
                tmpStr = parsedTlsRequest_.body.substr(pos, 4);
                tmpPos = hexToInt(tmpStr);
                pos += 4;
                tmpStr = parsedTlsRequest_.body.substr(pos, tmpPos * 2);
                parsedTlsRequest_.sni = hexToASCII(tmpStr);
            }
        }
        setIPPort();
        return true;
    } else if (tmpStr == "14") {
        parsedTlsRequest_.type = TlsTypes::ChangeCipherSpec;
        setIPPort();
        return true;
    } else if (tmpStr == "17") {
        parsedTlsRequest_.type = TlsTypes::ApplicationData;
        setIPPort();
        return true;
    } else
        return false;
}

const std::string HTTP::genHttpPostReqString(const std::string &body) const {
    return std::string(config_->general().method + " " +
                       config_->general().fakeUrl + " HTTP/" +
                       config_->agent().httpVersion + "\r\n") +
           "Host: " + config_->general().fakeUrl + "\r\n" +
           "User-Agent: " + config_->agent().userAgent + "\r\n" +
           "Accept: */*\r\n" + "Connection: keep-alive\r\n" +
           "Content-Length: " + std::to_string(body.length()) + "\r\n" +
           "Content-Type: application/x-www-form-urlencoded\r\n" + "\r\n" + body;
}

const std::string HTTP::genHttpRestPostReqString() const {
    return std::string(config_->general().method + " " +
                       config_->general().fakeUrl + " HTTP/" +
                       config_->agent().httpVersion + "\r\n") +
           "Host: " + config_->general().fakeUrl + "\r\n" +
           "User-Agent: " + config_->agent().userAgent + "\r\n" +
           "Accept: */*\r\n" + "Connection: keep-alive\r\n" +
           "Rest: yes\r\n";
}

const std::string HTTP::genHttpOkResString(const std::string &body) const {
    return std::string("HTTP/1.1 200 OK\r\n") +
           "Content-Type: application/x-www-form-urlencoded\r\n" +
           "Content-Length: " + std::to_string(body.length()) + "\r\n" +
           config_->general().chunkHeader + ": " + chunkHeader_ + "\r\n" +
           "Connection: keep-alive\r\n" + "Cache-Control: no-cache\r\n" +
           "Pragma: no-cache\r\n" + "\r\n" + body;
}

void HTTP::setIPPort() {
    std::string target{};
    std::vector<std::string> splitted;
    switch (httpType()) {
        case HTTP::HttpType::https:
            target = parsedTlsRequest_.sni;
            splitted = splitString(target, ":");
            if (splitted.size() > 1) {
                dstIP_ = splitted[0];
                dstPort_ = std::stoi(splitted[1]);
            } else {
                dstIP_ = target;
                dstPort_ = 443;
            }
            break;
        case HTTP::HttpType::http:
            target = boost::lexical_cast<std::string>(parsedHttpRequest_.target());
            splitted = splitString(
                    splitString(splitString(target, "http://")[1], "/")[0], ":");
            if (!splitted.empty()) {
                dstIP_ = splitted[0];
                if (splitted.size() > 1)
                    dstPort_ = std::stoi(splitted[1]);
                else
                    dstPort_ = 80;
            } else {
                log_->write("[" + to_string(uuid_) + "] [HTTP setIPPort] wrong request", Log::Level::DEBUG);
            }
            break;
        case HTTP::HttpType::connect:
            target = boost::lexical_cast<std::string>(parsedHttpRequest_.target());
            splitted = splitString(target, ":");
            dstIP_ = splitted[0];
            dstPort_ = std::stoi(splitted[1]);
            break;
    }
}

const std::string HTTP::tlsTypeToString() const {
    switch (parsedTlsRequest_.type) {
        case TlsTypes::TLSHandshake:
            return "TLSHandshake";
            break;
        case TlsTypes::ChangeCipherSpec:
            return "ChangeCipherSpec";
            break;
        case TlsTypes::ApplicationData:
            return "ApplicationData";
            break;
        default:
            return "UNKNOWN TLSTYPE";
    }
}

const std::string HTTP::toString() const {
    switch (httpType()) {
        case HTTP::HttpType::https:
            return std::string("\n") + "TLS Type : " + tlsTypeToString() + "\n" +
                   "SNI : " + parsedTlsRequest_.sni + "\n" + "Body Size : " +
                   boost::lexical_cast<std::string>(parsedTlsRequest_.body.size()) +
                   "\n" + "Body : " + parsedTlsRequest_.body + "\n";
            break;
        case HTTP::HttpType::http:
            return std::string("\n") + "Method : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.method()) +
                   "\n" + "Version : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.version()) +
                   "\n" + "Target : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.target()) +
                   "\n" + "User Agent : " +
                   boost::lexical_cast<std::string>(
                           parsedHttpRequest_["User-Agent"]) +
                   "\n" + "Body Size : " +
                   boost::lexical_cast<std::string>(
                           parsedHttpRequest_.body().size()) +
                   "\n" + "Body : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.body()) + "\n";
            break;
        case HTTP::HttpType::connect:
            return std::string("\n") + "Method : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.method()) +
                   "\n" + "Target : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.target()) +
                   "\n";
            break;
        default:
            return "UNKNOWN HTTPTYPE";
    }
}

const std::string HTTP::restoString() const {
    return std::string("\n") +
           boost::lexical_cast<std::string>(parsedHttpResponse_.base()) + "\n" +
           "Body Size : " +
           boost::lexical_cast<std::string>(parsedHttpResponse_.body().length()) +
           "\n" + "Body : " + parsedHttpResponse_.body() + "\n";
}