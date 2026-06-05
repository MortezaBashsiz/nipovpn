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

    enum class HttpType {
        https,
        http,
        connect
    };

    enum class TlsTypes {
        TLSHandshake,
        ChangeCipherSpec,
        ApplicationData
    };

    struct TlsRequest {
        std::string sni;
        std::string body;
        TlsTypes type;
    };

    explicit HTTP(const HTTP &request);

    ~HTTP();

    inline HTTP::HttpType httpType() const { return httpType_; }

    inline void httpType(const HTTP::HttpType &httpType) { httpType_ = httpType; }

    bool detectType();

    bool parseHttp();

    bool parseHttpResp();

    bool parseTls();

    std::string genHttpPostReqString(const std::string &body) const;

    std::string genHttpRestPostReqString() const;

    std::string genHttpOkResString(const std::string &body) const;

    inline const boost::beast::http::request<boost::beast::http::string_body> &
    parsedHttpRequest() & {
        return parsedHttpRequest_;
    }

    inline const boost::beast::http::response<boost::beast::http::string_body> &
    parsedHttpResponse() & {
        return parsedHttpResponse_;
    }

    inline const TlsRequest &parsedTlsRequest() & { return parsedTlsRequest_; }

    std::string tlsTypeToString() const;

    inline const std::string &dstIp() const { return dstIp_; }

    inline unsigned short dstPort() const { return dstPort_; }

    std::string toString() const;

    std::string restoString() const;

private:
    explicit HTTP(const std::shared_ptr<Config> &config,
                  const std::shared_ptr<Log> &log,
                  boost::asio::streambuf &buffer,
                  boost::uuids::uuid uuid);

    void setIPPort();

    std::shared_ptr<Config> config_;
    std::shared_ptr<Log> log_;
    boost::asio::streambuf &buffer_;

    boost::beast::http::parser<false, boost::beast::http::string_body> parser_;
    boost::beast::http::request<boost::beast::http::string_body> parsedHttpRequest_;
    boost::beast::http::response<boost::beast::http::string_body> parsedHttpResponse_;

    HttpType httpType_;
    std::string dstIp_;
    unsigned short dstPort_;
    TlsRequest parsedTlsRequest_;
    boost::uuids::uuid uuid_;
};