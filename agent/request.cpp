#include "request.hpp"

RequestHandler::RequestHandler(Config config) : nipoLog(config), nipoEncrypt(config) {
	nipoConfig = config;
}

void RequestHandler::handleRequest(request& req, response& res) 
{
	Proxy proxy(nipoConfig);
	unsigned char *encryptedData;
	std::string encodedData, decodedData;
	std::string data = boost::lexical_cast<std::string>(req.parsedRequest);
	int dataLen = data.length()+1;
	std::string logMsg = 	"vpn request, " 
												+ req.clientIP + ":" 
												+ req.clientPort;
	nipoLog.write(logMsg, nipoLog.levelInfo);
	nipoLog.write("Recieved request from client", nipoLog.levelDebug);
	nipoLog.write(req.toString(), nipoLog.levelDebug);
	encryptedData = nipoEncrypt.encryptAes((unsigned char *)data.c_str(), &dataLen);
	nipoLog.write("Encrypted request", nipoLog.levelDebug);
	nipoLog.write((char *)encryptedData, nipoLog.levelDebug);
	nipoLog.write("Encoding encrypted request", nipoLog.levelDebug);
	encodedData = nipoEncrypt.encode64((char *)encryptedData);
	nipoLog.write("Encoded encrypted request", nipoLog.levelDebug);
	nipoLog.write(encodedData, nipoLog.levelDebug);
	nipoLog.write("Sending request to nipoServer", nipoLog.levelDebug);
	std::string result = proxy.send(encodedData, dataLen);
	nipoLog.write("Response recieved from niposerver", nipoLog.levelDebug);
	nipoLog.write("\n"+result+"\n", nipoLog.levelDebug);
	response newResponse;
	newResponse.parse(result);
	nipoLog.write("Parsed response from niposerver", nipoLog.levelDebug);
	nipoLog.write(newResponse.toString(), nipoLog.levelDebug);
	int responseContentLength = std::stoi(newResponse.contentLength);
	decodedData = nipoEncrypt.decode64(newResponse.parsedResponse.body().data());
	nipoLog.write("Decoded recieved response", nipoLog.levelDebug);
	nipoLog.write(decodedData, nipoLog.levelDebug);
	char *plainData = (char *)nipoEncrypt.decryptAes((unsigned char *)decodedData.c_str(), &responseContentLength);
	nipoLog.write("Decrypt recieved response from niposerver", nipoLog.levelDebug);
	nipoLog.write(plainData, nipoLog.levelDebug);
	res.parse(plainData);
	res.content = res.encryptedContent;
	nipoLog.write("Sending response to client", nipoLog.levelDebug);
	nipoLog.write(res.toString(), nipoLog.levelDebug);
	return;
};


void request::parse(std::string request)
{
	boost::asio::io_context ctx;
	boost::process::async_pipe pipe(ctx);
	write(pipe, boost::asio::buffer(request));
	::close(pipe.native_sink());
	boost::beast::flat_buffer buf;
	boost::system::error_code ec;
	boost::beast::http::request<boost::beast::http::string_body> req;
	for (req; !ec && boost::beast::http::read(pipe, buf, req, ec); req.clear()) {
		parsedRequest = req;
		content = req.body().data();
		method = req.method();
		httpVersion = req.version();
		uri = req.target();
		userAgent = boost::lexical_cast<std::string>(req["User-Agent"]);
		contentLength = req["Content-Length"];
		std::vector<std::string> list = splitString(req["Host"], ':');
		host = list[0];
		if (list.size() == 2) 
		{
			port = list[1];
		}
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