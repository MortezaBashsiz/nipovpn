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

bool Request::detectType()
{
	std::string requestStr(
		hexArrToStr(
			reinterpret_cast<unsigned char*>(
				const_cast<char*>(
					streambufToString(buffer_).c_str()
				)
			),
			streambufToString(buffer_).length()
		)
	);
	std::string tmpStr;
	unsigned short pos=0;
	tmpStr = requestStr.substr(pos, 2);
	if (tmpStr == "16" || tmpStr == "14" || tmpStr == "17")
	{
		httpType(Request::HttpType::HTTPS);
		parsedTlsRequest_.body = requestStr;
		if (parseTls())
			return true;
		else
			return false;
	}
	else
	{
		httpType(Request::HttpType::HTTP);
		if(parseHttp())
			return true;
		else
			return false;
	}
}

bool Request::parseHttp()
{
	std::string requestStr(streambufToString(buffer_));
	boost::beast::http::request_parser<boost::beast::http::string_body> parser;
	parser.eager(true);
	boost::beast::error_code error;
	parser.put(boost::asio::buffer(requestStr), error);
	if (error)
	{
		log_->write(std::string("[Request parseHttp] ") + error.what(), Log::Level::DEBUG);
		return false;
	} else
	{
		parsedHttpRequest_ = parser.get();
		setIPPort();
		return true;
	}
}

bool Request::parseTls()
{
	std::string tmpStr;
	unsigned short pos=0;
	tmpStr = parsedTlsRequest_.body.substr(pos, 2);
	if (tmpStr == "16")
	{
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
		setIPPort();
		return true;
	}else	if (tmpStr == "14")
	{
		parsedTlsRequest_.type = TlsTypes::ChangeCipherSpec;
		setIPPort();
		return true;
	}else	if (tmpStr == "17")
	{
		parsedTlsRequest_.type = TlsTypes::ApplicationData;
		setIPPort();
		return true;
	} else 
		return false;
}

void Request::setIPPort()
{
	std::string target{};
	std::vector<std::string> splitted;
	switch (httpType()){
		case Request::HttpType::HTTPS:
			target = parsedTlsRequest_.sni;
			splitted = splitString(target, ":");
			if (splitted.size() > 1){
				dstIP_=splitted[0];
				dstPort_ = std::stoi(splitted[1]);
			} else
			{
				dstIP_ = target;
				dstPort_ = 443;
			}
		break;
		case Request::HttpType::HTTP:
			target = boost::lexical_cast<std::string>(parsedHttpRequest_.target());
			if (parsedHttpRequest_.method() == boost::beast::http::verb::connect)
			{
				splitted = splitString(target, ":");
				dstIP_=splitted[0];
				dstPort_=std::stoi(splitted[1]);
			} else
			{
				splitted = 	splitString(
											splitString(
												splitString(target, "http://")[1],
											"/")[0],
										":");
				if (!splitted.empty())
				{
					dstIP_=splitted[0];
					if (splitted.size() > 1)
						dstPort_ = std::stoi(splitted[1]);
					else
						dstPort_ = 80;
				}else
				{
					log_->write("[Request setIPPort] wrong request", Log::Level::ERROR);
				}
			}
		break;
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