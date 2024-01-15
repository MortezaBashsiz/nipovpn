#include "request.hpp"

Request::Request(const std::shared_ptr<Config>& config, const std::shared_ptr<Log>& log, boost::asio::streambuf& buffer)
	:
		config_(config),
		log_(log),
		buffer_(buffer),
		parsedHttpRequest_(),
		httpType_(Request::HttpType::HTTPS),
		parsedTlsRequest_
		{
			"",
			"",
			TlsTypes::TLSHandshake,
			TlsSteps::ClientHello
		}
{	}

Request::Request(const Request& request)
	:
		config_(request.config_),
		log_(request.log_),
		buffer_(request.buffer_),
		parsedHttpRequest_(request.parsedHttpRequest_),
		parsedTlsRequest_(request.parsedTlsRequest_)
{ }

Request::~Request()
{ }

const Request::HttpType& Request::httpType() const
{
	return httpType_;
}

void Request::httpType(const Request::HttpType& httpType)
{
	httpType_ = httpType;
}

void Request::detectType()
{
	std::string requestStr(streambufToString(buffer_));
	std::string tmpStr;
	unsigned short pos=0;
	tmpStr = requestStr.substr(pos, 2);
	if (tmpStr == "16" || tmpStr == "14" || tmpStr == "17")
	{
		httpType(Request::HttpType::HTTPS);
		parseTls();
	}
	else
	{
		httpType(Request::HttpType::HTTP);
		parseHttp();
	}
}

void Request::parseHttp()
{
	std::string requestStr(streambufToString(buffer_));
	boost::beast::http::request_parser<boost::beast::http::string_body> parser;
	parser.eager(true);
	boost::beast::error_code error;
	parser.put(boost::asio::buffer(requestStr), error);
	if (error)
	{
		log_->write(" [parseHttp] " + error.what(), Log::Level::ERROR);
	} else
	{
		parsedHttpRequest_ = parser.get();
	}
}

void Request::parseTls()
{
	parsedTlsRequest_.body = streambufToString(buffer_);
	std::string tmpStr;
	unsigned short pos=0;
	tmpStr = parsedTlsRequest_.body.substr(pos, 2);
	if (tmpStr == "16"){
		parsedTlsRequest_.type = TlsTypes::TLSHandshake;
		pos=10;
		tmpStr = parsedTlsRequest_.body.substr(pos, 2);
		if (tmpStr == "01"){
			unsigned short tmpPos(0);
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

const std::string Request::tlsTypeToString() const
{
	switch (parsedTlsRequest_.type){
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

const std::string Request::tlsStepToString() const
{
	switch (parsedTlsRequest_.step){
		case TlsSteps::ClientHello:
			return "ClientHello";
			break;
		case TlsSteps::ServerHello:
			return "ServerHello";
			break;
		case TlsSteps::ServerCertificate:
			return "ServerCertificate";
			break;
		case TlsSteps::ServerKeyExchange:
			return "ServerKeyExchange";
			break;
		case TlsSteps::ServerHelloDone:
			return "ServerHelloDone";
			break;
		case TlsSteps::ClientKeyExchange:
			return "ClientKeyExchange";
			break;
		case TlsSteps::ClientChangeCipherSpec:
			return "ClientChangeCipherSpec";
			break;
		case TlsSteps::ClientHandshakeFinished:
			return "ClientHandshakeFinished";
			break;
		case TlsSteps::ServerChangeCipherSpec:
			return "ServerChangeCipherSpec";
			break;
		case TlsSteps::ServerHandshakeFinished:
			return "ServerHandshakeFinished";
			break;
		case TlsSteps::ClientApplicationData:
			return "ClientApplicationData";
			break;
		case TlsSteps::ServerApplicationData:
			return "ServerApplicationData";
			break;
		case TlsSteps::ClientCloseNotify:
			return "ClientCloseNotify";
			break;
		default:
			return "UNKNOWN STEP";
	}
}

const std::string Request::toString() const
{
	switch (httpType()){
		case Request::HttpType::HTTPS:
			return std::string("\n")
							+ "TLS Type : " + tlsTypeToString() + "\n"
							+ "TLS Step : " + tlsStepToString() + "\n"
							+ "SNI : " + parsedTlsRequest_.sni + "\n"
							+ "Body : " + parsedTlsRequest_.body + "\n";
			break;
		case Request::HttpType::HTTP:
			return std::string("\n")
							+ "Method : " + boost::lexical_cast<std::string>(parsedHttpRequest_.method()) + "\n"
							+ "Version : " + boost::lexical_cast<std::string>(parsedHttpRequest_.version()) + "\n"
							+ "Target : " + boost::lexical_cast<std::string>(parsedHttpRequest_.target()) + "\n"
							+ "User Agent : " + boost::lexical_cast<std::string>(parsedHttpRequest_["User-Agent"]) + "\n"
							+ "Body Size : " + boost::lexical_cast<std::string>(parsedHttpRequest_.body().size()) + "\n";
			break;
		default:
			return "UNKNOWN HTTPTYPE";
	}
}