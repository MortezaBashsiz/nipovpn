#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <boost/beast/http.hpp>

/*
* This class is for handling request. When a request comes to TCPConnection(see tcp.hpp), It calls the 
* 	AgentHanler::handle function(see agenthandler.hpp) and object from this class will be created in AgentHanler::handle function
* 	to do all operations related to the request
*/
class Request : private Uncopyable
{
private:
	const Config& config_;
	Log log_;

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
	Request(const Config& config, boost::asio::streambuf& buffer):
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
		}
	{

	}

	/*
	* Copy constructor if you want to copy and initialize it
	*/
	Request(const Request& request):
		config_(request.config_),
		log_(request.config_),
		buffer_(request.buffer_),
		parsedHttpRequest_(request.parsedHttpRequest_),
		parsedTlsRequest_(request.parsedTlsRequest_)
	{}

	~Request()
	{}

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
			httpType_ = HttpType::HTTPS;
			parseTls();
		}
		else
		{
			httpType_ = HttpType::HTTP;
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

	HttpType httpType_ = HttpType::HTTPS;
	boost::asio::streambuf &buffer_;
	boost::beast::http::request<boost::beast::http::string_body> parsedHttpRequest_;
	TlsRequest parsedTlsRequest_;
};

#endif /* REQUEST_HPP */