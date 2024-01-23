#pragma once
#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <memory>

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "config.hpp"
#include "log.hpp"
#include "general.hpp"

/*
* This class is for handling request. When a request comes to TCPConnection(see tcp.hpp), It calls the
* 	AgentHanler::handle function(see agenthandler.hpp) and object from this class will be created in AgentHanler::handle function
* 	to do all operations related to the request
*/
class Request 
{
public:

	typedef std::shared_ptr<Request> pointer;

	static pointer create(const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log, boost::asio::streambuf& buffer)
	{
		return pointer(new Request(config, log, buffer));
	}

	/*
	* To define the type of HTTP/HTTPS request
	*/
	enum HttpType {
		HTTPS,
		HTTP
	};

	/*
	* To define the type of the TLS request
	*/
	enum TlsTypes {
		TLSHandshake,
		ChangeCipherSpec,
		ApplicationData
	};

	/*
	* To define the step of request in TLS handshake
	*/
	enum TlsSteps {
		ClientHello,
		ServerHello,
		ServerCertificate,
		ServerKeyExchange,
		ServerHelloDone,
		ClientKeyExchange,
		ClientChangeCipherSpec,
		ClientHandshakeFinished,
		ServerChangeCipherSpec,
		ServerHandshakeFinished,
		ClientApplicationData,
		ServerApplicationData,
		ClientCloseNotify
	};

	/*
	* To define the TLS request, This struct is used while parsing a request after we detect that it is a TLS Request
	*/
	struct TlsRequest {
		std::string sni;
		std::string body;
		TlsTypes type;
		TlsSteps step;
	};

	/*
	* Copy constructor if you want to copy and initialize it
	*/
	explicit Request(const Request& request);

	~Request();

	/*
	* Functions to handle private members
	*/
	const HttpType& httpType() const;

	void httpType(const HttpType& httpType);

	/*
	* This function will detect the type of request (HTTP|HTTPS)
	* It checks the first byte of the body, in case of 16, 14, 17 it is HTTPS and else it may be HTTP
	*/
	void detectType();

	/*
	* If the request is HTTP it will parse it and store in parsedHttpRequest_
	*/
	void parseHttp();

	/*
	* If the request is HTTPS it will parse it and store in parsedTlsRequest_
	*/
	void parseTls();

	inline boost::beast::http::request<boost::beast::http::string_body> parsedHttpRequest() const
	{
		return parsedHttpRequest_;
	}

	inline TlsRequest parsedTlsRequest() const
	{
		return parsedTlsRequest_;
	}

	/*
	* This function returns the string of parsedTlsRequest_.type
	*/
	const std::string tlsTypeToString() const;

	/*
	* This function returns the string of parsedTlsRequest_.step
	*/
	const std::string tlsStepToString() const;

	inline std::string dstIP() const
	{
		return dstIP_;
	}

	inline unsigned short dstPort() const
	{
		return dstPort_;
	}

	/*
	* This function returns string of request based on the HttpType (HTTP|HTTPS)
	*/
	const std::string toString() const;

private:
	/*
	* default constructor
	*/
	explicit Request(const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log, boost::asio::streambuf& buffer);

	/*
	* This function returns map of dst IP and Port for request
	*/
	void setIPPort();

	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	boost::asio::streambuf &buffer_;
	boost::beast::http::request<boost::beast::http::string_body> parsedHttpRequest_;
	HttpType httpType_;
	std::string dstIP_;
	unsigned short dstPort_;
	TlsRequest parsedTlsRequest_;
};

#endif /* REQUEST_HPP */