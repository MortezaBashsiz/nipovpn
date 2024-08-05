#pragma once
#ifndef HTTP_HPP
#define HTTP_HPP

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/lexical_cast.hpp>
#include <memory>

#include "config.hpp"
#include "general.hpp"
#include "log.hpp"

/*
 * This class is for handling request. When a request comes to TCPConnection(see
 * tcp.hpp), It calls the AgentHanler::handle function(see agenthandler.hpp) and
 * object from this class will be created in AgentHanler::handle function to do
 * all operations related to the request
 */
class HTTP {
 public:
  using pointer = std::shared_ptr<HTTP>;

  static pointer create(const std::shared_ptr<Config>& config,
                        const std::shared_ptr<Log>& log,
                        boost::asio::streambuf& buffer) {
    return pointer(new HTTP(config, log, buffer));
  }

  /*
   * To define the type of HTTP/HTTPS request
   */
  enum class HttpType { https, http, connect };

  /*
   * To define the type of the TLS request
   */
  enum class TlsTypes { TLSHandshake, ChangeCipherSpec, ApplicationData };

  /*
   * To define the TLS request, This struct is used while parsing a request
   * after we detect that it is a TLS HTTP
   */
  struct TlsRequest {
    std::string sni;
    std::string body;
    TlsTypes type;
  };

  /*
   * Copy constructor if you want to copy and initialize it
   */
  explicit HTTP(const HTTP& request);

  ~HTTP();

  /*
   * Functions to handle private members
   */
  inline const HTTP::HttpType& httpType() const { return httpType_; }

  inline void httpType(const HTTP::HttpType& httpType) { httpType_ = httpType; }

  /*
   * This function will detect the type of request (HTTP|HTTPS)
   * It checks the first byte of the body, in case of 16, 14, 17 it is HTTPS and
   * else it may be HTTP
   */
  bool detectType();

  /*
   * If the request is HTTP it will parse it and store in parsedHttpRequest_
   */
  bool parseHttp();
  bool parseHttpResp();

  /*
   * If the request is HTTPS it will parse it and store in parsedTlsRequest_
   */
  bool parseTls();

  const std::string genHttpPostReqString(const std::string& body) const;

  const std::string genHttpOkResString(const std::string& body) const;

  inline const boost::beast::http::request<boost::beast::http::string_body>&
  parsedHttpRequest() & {
    return parsedHttpRequest_;
  }

  inline const boost::beast::http::response<boost::beast::http::string_body>&
  parsedHttpResponse() & {
    return parsedHttpResponse_;
  }

  inline const TlsRequest& parsedTlsRequest() & { return parsedTlsRequest_; }

  /*
   * This function returns the string of parsedTlsRequest_.type
   */
  const std::string tlsTypeToString() const;

  inline const std::string& dstIP() & { return dstIP_; }
  inline const std::string&& dstIP() && { return std::move(dstIP_); }

  inline const unsigned short& dstPort() & { return dstPort_; }
  inline const unsigned short&& dstPort() && { return std::move(dstPort_); }

  /*
   * This function returns string of request based on the HttpType (HTTP|HTTPS)
   */
  const std::string toString() const;
  const std::string restoString() const;

 private:
  /*
   * default constructor
   */
  explicit HTTP(const std::shared_ptr<Config>& config,
                const std::shared_ptr<Log>& log,
                boost::asio::streambuf& buffer);

  /*
   * This function returns map of dst IP and Port for request
   */
  void setIPPort();

  const std::shared_ptr<Config>& config_;
  const std::shared_ptr<Log>& log_;
  boost::asio::streambuf& buffer_;
  boost::beast::http::parser<false, boost::beast::http::string_body> parser_;
  boost::beast::http::request<boost::beast::http::string_body>
      parsedHttpRequest_;
  boost::beast::http::response<boost::beast::http::string_body>
      parsedHttpResponse_;
  HttpType httpType_;
  std::string dstIP_;
  unsigned short dstPort_;
  TlsRequest parsedTlsRequest_;
};

#endif /* HTTP_HPP */