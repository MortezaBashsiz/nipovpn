#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <boost/beast/http.hpp>
#include <boost/lexical_cast.hpp>

/*
* This class is for handling request. When a request comes to TCPConnection(see tcp.hpp), It calls the 
* 	AgentHanler::handle function(see agenthandler.hpp) and object from this class will be created in AgentHanler::handle function
* 	to do all operations related to the request
*/
class Request : private Uncopyable
{
public:

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
	* default constructor
	*/
	explicit Request(const Config& config, boost::asio::streambuf& buffer):
		config_(config),
		log_(config),
		buffer_(buffer),
		parsedHttpRequest_(),
		parsedTlsRequest_ 
		{
			"",
			"",
			TlsTypes::TLSHandshake,
			TlsSteps::ClientHello
		},
		httpType_(HttpType::HTTPS)
	{

	}

	/*
	* Copy constructor if you want to copy and initialize it
	*/
	explicit Request(const Request& request):
		config_(request.config_),
		log_(request.config_),
		buffer_(request.buffer_),
		parsedHttpRequest_(request.parsedHttpRequest_),
		parsedTlsRequest_(request.parsedTlsRequest_)
	{}

	~Request()
	{}

	/*
	* Functions to handle private members
	*/
	const HttpType& httpType() const
	{
		return httpType_;
	}

	void httpType(const HttpType& httpType)
	{
		httpType_ = httpType;
	}

	/*
	* This function will detect the type of request (HTTP|HTTPS)
	* It checks the first byte of the body, in case of 16, 14, 17 it is HTTPS and else it may be HTTP
	*/
	void detectType()
	{
		std::string requestStr(streambufToString(buffer_));
		std::string tmpStr;
		unsigned short pos=0;
		tmpStr = requestStr.substr(pos, 2);
		if (tmpStr == "16" || tmpStr == "14" || tmpStr == "17")
		{
			httpType(HttpType::HTTPS);
			parseTls();
		}
		else
		{
			httpType(HttpType::HTTP);
			parseHttp();
		}
	}

	/*
	* If the request is HTTP it will parse it and store in parsedHttpRequest_
	*/
	void parseHttp()
	{
		std::string requestStr(streambufToString(buffer_));
		boost::beast::http::request_parser<boost::beast::http::string_body> parser;
		parser.eager(true);
		boost::beast::error_code error;
		parser.put(boost::asio::buffer(requestStr), error);
		if (error)
		{
			log_.write(" [parseHttp] " + error.what(), Log::Level::ERROR);
		} else
		{
			parsedHttpRequest_ = parser.get();
		}
	}

	/*
	* If the request is HTTPS it will parse it and store in parsedTlsRequest_
	*/
	void parseTls()
	{
		parsedTlsRequest_.body = streambufToString(buffer_);
		std::string tmpStr;
		unsigned short tmpPos;
		unsigned short pos=0;
		tmpStr = parsedTlsRequest_.body.substr(pos, 2);
		if (tmpStr == "16"){
			parsedTlsRequest_.type = TlsTypes::TLSHandshake;
			pos=10;
			tmpStr = parsedTlsRequest_.body.substr(pos, 2);
			if (tmpStr == "01"){
				parsedTlsRequest_.step = TlsSteps::ClientHello;		
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
				if ( hexToInt(tmpStr) == 0 ){
					pos += 14;
					tmpStr = parsedTlsRequest_.body.substr(pos, 4);
					tmpPos = hexToInt(tmpStr);
					pos += 4;
					tmpStr = parsedTlsRequest_.body.substr(pos, tmpPos * 2);
					parsedTlsRequest_.sni = hexToASCII(tmpStr);
				}
			}
		}
		if (tmpStr == "14"){
			parsedTlsRequest_.type = TlsTypes::ChangeCipherSpec;
		}
		if (tmpStr == "17"){
			parsedTlsRequest_.type = TlsTypes::ApplicationData;
		}
	}

	/*
	* This function returns the string of parsedTlsRequest_.type
	*/
	const std::string tlsTypeToString() const
	{
		std::string result("");
		switch (parsedTlsRequest_.type){
			case TlsTypes::TLSHandshake:
				result = "TLSHandshake";
				break;
			case TlsTypes::ChangeCipherSpec:
				result = "ChangeCipherSpec";
				break;
			case TlsTypes::ApplicationData:
				result = "ApplicationData";
				break;
		}
		return result;
	}

	/*
	* This function returns the string of parsedTlsRequest_.step
	*/
	const std::string tlsStepToString() const
	{
		std::string result("");
		switch (parsedTlsRequest_.step){
			case TlsSteps::ClientHello:
				result = "ClientHello";
				break;
			case TlsSteps::ServerHello:
				result = "ServerHello";
				break;
			case TlsSteps::ServerCertificate:
				result = "ServerCertificate";
				break;
			case TlsSteps::ServerKeyExchange:
				result = "ServerKeyExchange";
				break;
			case TlsSteps::ServerHelloDone:
				result = "ServerHelloDone";
				break;
			case TlsSteps::ClientKeyExchange:
				result = "ClientKeyExchange";
				break;
			case TlsSteps::ClientChangeCipherSpec:
				result = "ClientChangeCipherSpec";
				break;
			case TlsSteps::ClientHandshakeFinished:
				result = "ClientHandshakeFinished";
				break;
			case TlsSteps::ServerChangeCipherSpec:
				result = "ServerChangeCipherSpec";
				break;
			case TlsSteps::ServerHandshakeFinished:
				result = "ServerHandshakeFinished";
				break;
			case TlsSteps::ClientApplicationData:
				result = "ClientApplicationData";
				break;
			case TlsSteps::ServerApplicationData:
				result = "ServerApplicationData";
				break;
			case TlsSteps::ClientCloseNotify:
				result = "ClientCloseNotify";
				break;
		}
		return result;
	}

	/*
	* This function returns string of request based on the HttpType (HTTP|HTTPS)
	*/
	const std::string toString() const
	{
		std::string tmpStr("");
		if (httpType() == Request::HttpType::HTTPS)
		{
			tmpStr = 	std::string("\n")
								+ "TLS Type : " + tlsTypeToString() + "\n"
								+ "TLS Step : " + tlsStepToString() + "\n"
								+ "SNI : " + parsedTlsRequest_.sni + "\n"
								+ "Body : " + parsedTlsRequest_.body + "\n";
		} else 
		{
			tmpStr = 	std::string("\n")
								+ "Method : " + boost::lexical_cast<std::string>(parsedHttpRequest_.method()) + "\n"
								+ "Version : " + boost::lexical_cast<std::string>(parsedHttpRequest_.version()) + "\n"
								+ "Target : " + boost::lexical_cast<std::string>(parsedHttpRequest_.target()) + "\n"
								+ "User Agent : " + boost::lexical_cast<std::string>(parsedHttpRequest_["User-Agent"]) + "\n"
								+ "Body Size : " + boost::lexical_cast<std::string>(parsedHttpRequest_.body().size()) + "\n";
		}
		return tmpStr;
	}

private:
	const Config& config_;
	Log log_;
	HttpType httpType_;
	boost::asio::streambuf &buffer_;
	boost::beast::http::request<boost::beast::http::string_body> parsedHttpRequest_;
	TlsRequest parsedTlsRequest_;

};

#endif /* REQUEST_HPP */