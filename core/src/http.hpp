#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/lexical_cast.hpp>
#include <memory>

#include "config.hpp"
#include "general.hpp"
#include "log.hpp"

class HTTP {
public:
    using pointer = std::shared_ptr<HTTP>;

    static pointer create(const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          boost::asio::streambuf &buffer,
                          boost::uuids::uuid uuid) {
        return pointer(new HTTP(config, log, buffer, uuid));
    }

    enum class HttpType { https,
                          http,
                          connect };

    enum class TlsTypes { TLSHandshake,
                          ChangeCipherSpec,
                          ApplicationData };

    struct TlsRequest {
        std::string sni;
        std::string body;
        TlsTypes type;
    };

    explicit HTTP(const HTTP &request);

    ~HTTP();

    inline const HTTP::HttpType &httpType() const { return httpType_; }

    inline void httpType(const HTTP::HttpType &httpType) { httpType_ = httpType; }

    bool detectType();

    bool parseHttp();
    bool parseHttpResp();

    bool parseTls();

    const std::string genHttpPostReqString(const std::string &body) const;

    const std::string genHttpRestPostReqString() const;

    const std::string genHttpOkResString(const std::string &body) const;

    inline const boost::beast::http::request<boost::beast::http::string_body> &
    parsedHttpRequest() & {
        return parsedHttpRequest_;
    }

    inline const boost::beast::http::response<boost::beast::http::string_body> &
    parsedHttpResponse() & {
        return parsedHttpResponse_;
    }

    inline const TlsRequest &parsedTlsRequest() & { return parsedTlsRequest_; }

    const std::string tlsTypeToString() const;

    inline const std::string &dstIP() & { return dstIP_; }
    inline const std::string &&dstIP() && { return std::move(dstIP_); }

    inline const unsigned short &dstPort() & { return dstPort_; }
    inline const unsigned short &&dstPort() && { return std::move(dstPort_); }

    const std::string toString() const;
    const std::string restoString() const;
    std::string chunkHeader_;

private:
    explicit HTTP(const std::shared_ptr<Config> &config,
                  const std::shared_ptr<Log> &log,
                  boost::asio::streambuf &buffer,
                  boost::uuids::uuid uuid);

    void setIPPort();

    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    boost::asio::streambuf &buffer_;
    boost::beast::http::parser<false, boost::beast::http::string_body> parser_;
    boost::beast::http::request<boost::beast::http::string_body>
            parsedHttpRequest_;
    boost::beast::http::response<boost::beast::http::string_body>
            parsedHttpResponse_;
    HttpType httpType_;
    std::string dstIP_;
    unsigned short dstPort_;
    TlsRequest parsedTlsRequest_;
    boost::uuids::uuid uuid_;
};
