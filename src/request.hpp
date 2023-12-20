#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <boost/beast/http.hpp>

class Request : private Uncopyable
{
private:
	const Config& config_;
	Log log_;

public:
	enum HttpType {
		HTTPS,
		HTTP
	};

	enum TlsTypes {
		TLSHandshake,
		ChangeCipherSpec,
		ApplicationData
	};

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

	struct TlsRequest {
		std::string sni;
		std::string body;
		TlsTypes type;
		TlsSteps step;
	};

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