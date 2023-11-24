#include "request.hpp"

RequestHandler::RequestHandler(Config config) : nipoLog(config), nipoEncrypt(config) {
	nipoConfig = config;
}

void RequestHandler::handleRequest(request& req, response& res) 
{
	Proxy proxy(nipoConfig);
	unsigned char *encryptedData;
	std::string encodedData, decodedData;
	response newResponse;

	std::string data = boost::lexical_cast<std::string>(req.parsedRequest);
	int dataLen = data.length()+1;
	std::string logMsg = 	"vpn request " 
												+ req.clientIP + ":" 
												+ req.clientPort + " "
												+ boost::lexical_cast<std::string>(req.method) + " "
												+ req.httpVersion + " "
												+ req.serverName + " "
												+ req.userAgent + " "
												+ req.contentLength;
	nipoLog.write(logMsg, nipoLog.levelInfo);
	nipoLog.write("Recieved request from client", nipoLog.levelDebug);
	nipoLog.write(req.toString(), nipoLog.levelDebug);
	if (nipoConfig.config.encryption == "yes")
	{
		encryptedData = nipoEncrypt.encryptAes((unsigned char *)data.c_str(), &dataLen);
		nipoLog.write("Encrypted request", nipoLog.levelDebug);
		nipoLog.write((char *)encryptedData, nipoLog.levelDebug);
		nipoLog.write("Encoding encrypted request", nipoLog.levelDebug);
		encodedData = nipoEncrypt.encode64((char *)encryptedData);
	} else if(nipoConfig.config.encryption == "no") {
			encodedData = nipoEncrypt.encode64(data);
	}
	nipoLog.write("Encoded encrypted request", nipoLog.levelDebug);
	nipoLog.write(encodedData, nipoLog.levelDebug);
	nipoLog.write("Sending request to nipoServer", nipoLog.levelDebug);
	
	proxy.request = encodedData;
	proxy.dataLen = dataLen;
	proxy.serverName = req.serverName;
	proxy.send();
	
	nipoLog.write("Response recieved from niposerver", nipoLog.levelDebug);
	nipoLog.write("\n"+proxy.response+"\n", nipoLog.levelDebug);
	newResponse.parse(proxy.response);
	nipoLog.write("Parsed response from niposerver", nipoLog.levelDebug);
	nipoLog.write(newResponse.toString(), nipoLog.levelDebug);
	int responseContentLength;
	if ( newResponse.contentLength != "" ){
		responseContentLength = std::stoi(newResponse.contentLength);
		decodedData = nipoEncrypt.decode64(newResponse.parsedResponse.body().data());
	} else{
		responseContentLength = 0;
		decodedData = "Empty";
	}
	nipoLog.write("Decoded recieved response", nipoLog.levelDebug);
	nipoLog.write(decodedData, nipoLog.levelDebug);
	if (nipoConfig.config.encryption == "yes")
	{
		char *plainData = (char *)nipoEncrypt.decryptAes((unsigned char *)decodedData.c_str(), &responseContentLength);
		nipoLog.write("Decrypt recieved response from niposerver", nipoLog.levelDebug);
		nipoLog.write(plainData, nipoLog.levelDebug);
		res.parse(plainData);
	} else if(nipoConfig.config.encryption == "no") {
		res.parse(decodedData);
	}
	nipoLog.write("Sending response to client", nipoLog.levelDebug);
	nipoLog.write(res.toString(), nipoLog.levelDebug);
	return;
};

void request::parse(std::string request)
{
	boost::asio::io_context ctx;
	boost::system::error_code ec;
	boost::beast::http::request_parser<boost::beast::http::string_body> parser;
	parser.eager(true);
	parser.put(boost::asio::buffer(request), ec);
	boost::beast::http::request<boost::beast::http::string_body> req = parser.get();
	parsedRequest = req;
	content = req.body().data();
	method = req.method();
	httpVersion = req.version();
	serverName = req.target();
	userAgent = boost::lexical_cast<std::string>(req["User-Agent"]);
	contentLength = req["Content-Length"];
	std::vector<std::string> list = splitString(req["Host"], ':');
	host = list[0];
	if (list.size() == 2) 
	{
		port = list[1];
	}
};

bool tolowerCompare(char a, char b)
{
	return std::tolower(a) == std::tolower(b);
}

bool headersEqual(const std::string& a, const std::string& b)
{
	if (a.length() != b.length())
		return false;

	return std::equal(a.begin(), a.end(), b.begin(),
			&tolowerCompare);
}