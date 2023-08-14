#include "request.hpp"
#include "response.hpp"
#include "header.hpp"
#include "encrypt.hpp"
#include "proxy.hpp"
#include "general.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

RequestHandler::RequestHandler(Config config, const std::string& docRoot) : docRoot_(docRoot), nipoLog(config), nipoEncrypt(config) {
	nipoConfig = config;
}

void RequestHandler::handleRequest(request& req, response& res) {
	std::string requestPath;
	for (int counter=0; counter < nipoConfig.config.usersCount; counter+=1){
		std::string reqPath = nipoConfig.config.users[counter].endpoint;
		if (req.uri == reqPath) {
			int contentLengthInt = std::stoi(req.contentLength);
			nipoLog.write("Recieve request from nipoagent", nipoLog.levelDebug);
			nipoLog.write(req.toString(), nipoLog.levelDebug);
			std::cout << "REQ BODY: " << std::endl << req.content << std::endl;
			std::string plainData = (char *)(nipoEncrypt.decryptAes((unsigned char *)req.content.c_str(), &contentLengthInt));
			nipoLog.write("Decrypt request from nipoagent", nipoLog.levelDebug);
			nipoLog.write(plainData, nipoLog.levelDebug);
			request originalRequest;
			nipoLog.write("Parsing request from nipoagent", nipoLog.levelDebug);
			originalRequest.parse(plainData);
			nipoLog.write("Parsed request from nipoagent", nipoLog.levelDebug);
			nipoLog.write(originalRequest.toString(), nipoLog.levelDebug);
			if (originalRequest.method == boost::beast::http::verb::unknown)
			{
				res = response::stockResponse(response::badRequest);
				std::string logMsg = 	"request, " 
															+ originalRequest.clientIP + ":" 
															+ originalRequest.clientPort + ", " 
															+ boost::lexical_cast<std::string>(originalRequest.method) + ", " 
															+ originalRequest.uri + ", " 
															+ to_string(res.content.size()) + ", " 
															+ res.statusToString(res.status);
				nipoLog.write(logMsg , nipoLog.levelInfo);
				return;
			}
			Proxy proxy(nipoConfig);
			nipoLog.write("Send request to the originserver ", nipoLog.levelDebug);
			std::string newResponse = proxy.send(originalRequest);
			nipoLog.write("Response recieved from original server ", nipoLog.levelDebug);
			nipoLog.write("\n"+newResponse+"\n", nipoLog.levelDebug);
			int newRequestLength = newResponse.length();
			unsigned char *encryptedData;
			encryptedData = nipoEncrypt.encryptAes((unsigned char *)newResponse.c_str(), &newRequestLength);
			nipoLog.write("Encrypt response from originserver", nipoLog.levelDebug);
			nipoLog.write((char *)encryptedData, nipoLog.levelDebug);
			nipoLog.write("Generating response for nipoAgent ", nipoLog.levelDebug);
			res.content = (char *)encryptedData;
			res.status = response::ok;
			res.headers.resize(3);
			res.headers[1].name = "Accept";
			res.headers[1].value = "*/*";
			res.headers[0].name = "Content-Length";
			res.headers[0].value = std::to_string(newRequestLength);
			res.headers[2].name = "Content-Type";
			res.headers[2].value = "application/javascript";
			nipoLog.write("Response generated for nipoAgent ", nipoLog.levelDebug);
			nipoLog.write(res.toString(), nipoLog.levelDebug);
			std::string logMsg = 	"vpn request, " 
														+ req.clientIP + ":" 
														+ req.clientPort + ", " 
														+ boost::lexical_cast<std::string>(originalRequest.method) + ", " 
														+ originalRequest.uri + ", "
														+ originalRequest.host + ", "
														+ originalRequest.port + ", " 
														+ to_string(req.content.size()) + ", " 
														+ res.statusToString(res.status);
			nipoLog.write(logMsg , nipoLog.levelInfo);
			return;
		};
	};

	if (!urlDecode(req.uri, requestPath)) {
		res = response::stockResponse(response::badRequest);
		std::string logMsg = 	"request, " 
													+ req.clientIP + ":" 
													+ req.clientPort + ", " 
													+ boost::lexical_cast<std::string>(req.method) + ", " 
													+ req.uri + ", " 
													+ to_string(res.content.size()) + ", " 
													+ res.statusToString(res.status);
		nipoLog.write(logMsg , nipoLog.levelInfo);
		return;
	};

	if (requestPath.empty() || requestPath[0] != '/' || requestPath.find("..") != std::string::npos) {
		res = response::stockResponse(response::badRequest);
		std::string logMsg = 	"request, " 
													+ req.clientIP + ":" 
													+ req.clientPort + ", " 
													+ boost::lexical_cast<std::string>(req.method) + ", " 
													+ req.uri + ", " 
													+ to_string(res.content.size()) + ", " 
													+ res.statusToString(res.status);
		nipoLog.write(logMsg , nipoLog.levelInfo);
		return;
	};

	if (requestPath[requestPath.size() - 1] == '/') {
		requestPath += "index.html";
	};

	std::size_t lastSlashPos = requestPath.find_last_of("/");
	std::size_t lastDotPos = requestPath.find_last_of(".");
	std::string extension;
	if (lastDotPos != std::string::npos && lastDotPos > lastSlashPos) {
		extension = requestPath.substr(lastDotPos + 1);
	};

	std::string fullPath = docRoot_ + requestPath;
	std::ifstream is(fullPath.c_str(), std::ios::in | std::ios::binary);
	if (!is) {
		res = response::stockResponse(response::notFound);
		std::string logMsg = 	"request, " 
													+ req.clientIP + ":" 
													+ req.clientPort + ", " 
													+ boost::lexical_cast<std::string>(req.method) + ", " 
													+ req.uri + ", " 
													+ to_string(res.content.size()) + ", " 
													+ res.statusToString(res.status);
		nipoLog.write(logMsg , nipoLog.levelInfo);
		return;
	};
	res.status = response::ok;
	char buf[512];
	while (is.read(buf, sizeof(buf)).gcount() > 0)
		res.content.append(buf, is.gcount());
	res.headers.resize(2);
	res.headers[0].name = "Content-Length";
	res.headers[0].value = std::to_string(res.content.size());
	res.headers[1].name = "Content-Type";
	res.headers[1].value = mimeExtensionToType(extension);
	std::string logMsg = 	"request, "
												+ req.clientIP + ":" 
												+ req.clientPort + ", "
												+ boost::lexical_cast<std::string>(req.method) + ", " 
												+ req.uri + ", " 
												+ to_string(res.content.size()) + ", "
												+ res.statusToString(res.status);
	nipoLog.write(logMsg , nipoLog.levelInfo);
};

bool RequestHandler::urlDecode(const std::string& in, std::string& out) {
	out.clear();
	out.reserve(in.size());
	for (std::size_t i = 0; i < in.size(); ++i) {
		if (in[i] == '%') {
			if (i + 3 <= in.size()) {
				int value = 0;
				std::istringstream is(in.substr(i + 1, 2));
				if (is >> std::hex >> value) {
					out += static_cast<char>(value);
					i += 2;
				} else {
					return false;
				};
			} else {
				return false;
			};
		} else if (in[i] == '+') {
			out += ' ';
		} else {
			out += in[i];
		};
	};
	return true;
}

void request::parse(std::string request)
{
	std::cout << "BEFOR PARSE " << std::endl << request << std::endl;
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
		if(ec && ec != boost::beast::errc::not_connected)
				throw boost::beast::system_error{ec};
	}
	std::cout << "AFTER PARSE " << std::endl << boost::lexical_cast<std::string>(req) << std::endl;
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