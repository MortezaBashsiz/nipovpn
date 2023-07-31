#include "request.hpp"
#include "response.hpp"
#include "header.hpp"
#include "encrypt.hpp"

RequestHandler::RequestHandler(Config config, const std::string& docRoot) : docRoot_(docRoot), nipoLog(config), nipoEncrypt(config) {
	nipoConfig = config;
}

void RequestHandler::handleRequest(request& req, response& resp) {
	std::string requestPath;

	for (int counter=0; counter < nipoConfig.config.usersCount; counter+=1){
		std::string reqPath = nipoConfig.config.users[counter].endpoint;
		if (req.uri.rfind(reqPath, 0) == 0) {
			unsigned first = req.requestBody.content.find("DATA_START:");
			unsigned last = req.requestBody.content.find(":DATA_END");
			if ( req.headers.size() < 5 ){
				resp = response::stockResponse(response::badRequest);
			} else {
				if ( first == 4294967295 || last == 4294967295 ) {
					resp = response::stockResponse(response::badRequest);
				} else {
					resp.status = response::ok;
				};
			};
			std::string tempString = req.requestBody.content.substr(first+11,last-first+11);
			req.requestBody.content = tempString;
			int dataLenght = std::stoi(req.headers[4].value);
			char *plainData = (char *)nipoEncrypt.decryptAes(nipoEncrypt.decryptEvp, (unsigned char *) req.requestBody.content.c_str(), &dataLenght);
			resp.responseBody.content = plainData;
			std::string logMsg = 	"vpn request, " 
														+ req.clientIP + ":" 
														+ req.clientPort + ", " 
														+ req.method + ", " 
														+ req.uri + ", " 
														+ to_string(resp.responseBody.content.size()) + ", " 
														+ statusToString(resp.status);
			nipoLog.write(logMsg , nipoLog.levelInfo);
			return;
		};
	};

	if (!urlDecode(req.uri, requestPath)) {
		resp = response::stockResponse(response::badRequest);
		std::string logMsg = 	"request, " 
													+ req.clientIP + ":" 
													+ req.clientPort + ", " 
													+ req.method + ", " 
													+ req.uri + ", " 
													+ to_string(resp.responseBody.content.size()) + ", " 
													+ statusToString(resp.status);
		nipoLog.write(logMsg , nipoLog.levelInfo);
		return;
	};

	if (requestPath.empty() || requestPath[0] != '/' || requestPath.find("..") != std::string::npos) {
		resp = response::stockResponse(response::badRequest);
		std::string logMsg = 	"request, " 
													+ req.clientIP + ":" 
													+ req.clientPort + ", " 
													+ req.method + ", " 
													+ req.uri + ", " 
													+ to_string(resp.responseBody.content.size()) + ", " 
													+ statusToString(resp.status);
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
		resp = response::stockResponse(response::notFound);
		std::string logMsg = 	"request, " 
													+ req.clientIP + ":" 
													+ req.clientPort + ", " 
													+ req.method + ", " 
													+ req.uri + ", " 
													+ to_string(resp.responseBody.content.size()) + ", " 
													+ statusToString(resp.status);
		nipoLog.write(logMsg , nipoLog.levelInfo);
		return;
	};
	resp.status = response::ok;
	char buf[512];
	while (is.read(buf, sizeof(buf)).gcount() > 0)
		resp.responseBody.content.append(buf, is.gcount());
	resp.headers.resize(2);
	resp.headers[0].name = "Content-Length";
	resp.headers[0].value = std::to_string(resp.responseBody.content.size());
	resp.headers[1].name = "Content-Type";
	resp.headers[1].value = mimeExtensionToType(extension);
	std::string logMsg = 	"request, "
												+ req.clientIP + ":" 
												+ req.clientPort + ", "
												+ req.method + ", " 
												+ req.uri + ", " 
												+ to_string(resp.responseBody.content.size()) + ", "
												+ statusToString(resp.status);
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

RequestParser::RequestParser() : state_(methodStart) {
}

void RequestParser::reset() {
	state_ = methodStart;
}

RequestParser::resultType RequestParser::consume(request& req, char input) {
	switch (state_) {
	case methodStart:
		if (!isChar(input) || isCtl(input) || isTspecial(input)) {
			return bad;
		} else {
			state_ = method;
			req.method.push_back(input);
			return indeterminate;
		}
	case method:
		if (input == ' ') {
			state_ = uri;
			return indeterminate;
		} else if (!isChar(input) || isCtl(input) || isTspecial(input)) {
			return bad;
		} else {
			req.method.push_back(input);
			return indeterminate;
		}
	case uri:
		if (input == ' ') {
			state_ = httpVersionH;
			return indeterminate;
		}
		else if (isCtl(input)) {
			return bad;
		}
		else {
			req.uri.push_back(input);
			return indeterminate;
		}
	case httpVersionH:
		if (input == 'H') {
			state_ = httpVersionT1;
			return indeterminate;
		}
		else {
			return bad;
		}
	case httpVersionT1:
		if (input == 'T') {
			state_ = httpVersionT2;
			return indeterminate;
		}
		else {
			return bad;
		}
	case httpVersionT2:
		if (input == 'T') {
			state_ = httpVersionP;
			return indeterminate;
		}
		else {
			return bad;
		}
	case httpVersionP:
		if (input == 'P') {
			state_ = httpVersionSlash;
			return indeterminate;
		}
		else {
			return bad;
		}
	case httpVersionSlash:
		if (input == '/') {
			req.httpVersionMajor = 0;
			req.httpVersionMinor = 0;
			state_ = httpVersionMajorStart;
			return indeterminate;
		}
		else {
			return bad;
		}
	case httpVersionMajorStart:
		if (isDigit(input)) {
			req.httpVersionMajor = req.httpVersionMajor * 10 + input - '0';
			state_ = httpVersionMajor;
			return indeterminate;
		}
		else {
			return bad;
		}
	case httpVersionMajor:
		if (input == '.') {
			state_ = httpVersionMinorStart;
			return indeterminate;
		}
		else if (isDigit(input)) {
			req.httpVersionMajor = req.httpVersionMajor * 10 + input - '0';
			return indeterminate;
		}
		else {
			return bad;
		}
	case httpVersionMinorStart:
		if (isDigit(input)) {
			req.httpVersionMinor = req.httpVersionMinor * 10 + input - '0';
			state_ = httpVersionMinor;
			return indeterminate;
		}
		else {
			return bad;
		}
	case httpVersionMinor:
		if (input == '\r') {
			state_ = expectingNewline1;
			return indeterminate;
		}
		else if (isDigit(input)) {
			req.httpVersionMinor = req.httpVersionMinor * 10 + input - '0';
			return indeterminate;
		}
		else {
			return bad;
		}
	case expectingNewline1:
		if (input == '\n') {
			state_ = headerLineStart;
			return indeterminate;
		}
		else {
			return bad;
		}
	case headerLineStart:
		if (input == '\r') {
			state_ = expectingNewline3;
			return indeterminate;
		}
		else if (!req.headers.empty() && (input == ' ' || input == '\t')) {
			state_ = headerLws;
			return indeterminate;
		}
		else if (!isChar(input) || isCtl(input) || isTspecial(input)) {
			return bad;
		}
		else {
			req.headers.push_back(header());
			req.headers.back().name.push_back(input);
			state_ = headerName;
			return indeterminate;
		}
	case headerLws:
		if (input == '\r') {
			state_ = expectingNewline2;
			return indeterminate;
		}
		else if (input == ' ' || input == '\t') {
			return indeterminate;
		}
		else if (isCtl(input)) {
			return bad;
		}
		else {
			state_ = headerValue;
			req.headers.back().value.push_back(input);
			return indeterminate;
		}
	case headerName:
		if (input == ':') {
			state_ = spaceBeforeHeaderValue;
			return indeterminate;
		}
		else if (!isChar(input) || isCtl(input) || isTspecial(input)) {
			return bad;
		}
		else {
			req.headers.back().name.push_back(input);
			return indeterminate;
		}
	case spaceBeforeHeaderValue:
		if (input == ' ') {
			state_ = headerValue;
			return indeterminate;
		}
		else {
			return bad;
		}
	case headerValue:
		if (input == '\r') {
			state_ = expectingNewline2;
			return indeterminate;
		}
		else if (isCtl(input)) {
			return bad;
		}
		else {
			req.headers.back().value.push_back(input);
			return indeterminate;
		}
	case expectingNewline2:
		if (input == '\n') {
			state_ = headerLineStart;
			return indeterminate;
		}
		else {
			return bad;
		}
	case expectingNewline3:
		return (input == '\n') ? good : bad;
	default:
		return bad;
	}
}

bool RequestParser::isChar(int c) {
	return c >= 0 && c <= 127;
}

bool RequestParser::isCtl(int c) {
	return (c >= 0 && c <= 31) || (c == 127);
}

bool RequestParser::isTspecial(int c) {
	switch (c)
	{
	case '(': case ')': case '<': case '>': case '@':
	case ',': case ';': case ':': case '\\': case '"':
	case '/': case '[': case ']': case '?': case '=':
	case '{': case '}': case ' ': case '\t':
		return true;
	default:
		return false;
	}
}

bool RequestParser::isDigit(int c) {
	return c >= '0' && c <= '9';
}