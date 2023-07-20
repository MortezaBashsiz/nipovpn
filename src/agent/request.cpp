#include "request.hpp"
#include "response.hpp"
#include "header.hpp"
#include "aes.hpp"
#include "proxy.hpp"

agentRequestHandler::agentRequestHandler(agentConfig config) : nipoLog(config){
	nipoConfig = config;
}

void agentRequestHandler::handleRequest(request& req, response& resp) {
	if (req.method == "CONNECT"){
		resp.status = response::ok;
		cout << "ADAS : " << req.requestBody.content << std::endl << std::endl << std::endl ;
		unsigned int plainLen = 16 * (sizeof(req.requestBody.content));
		std::string encryptedBody = encrypt(req.requestBody.content, plainLen, nipoConfig.config.token);
		std::string decryptedBody = decrypt(encryptedBody, plainLen, nipoConfig.config.token);
	} else {
		resp = response::stockResponse(response::badRequest);
	};
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

bool agentRequestHandler::urlDecode(const std::string& in, std::string& out) {
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

agentRequestParser::agentRequestParser() : state_(methodStart) {
}

void agentRequestParser::reset() {
	state_ = methodStart;
}

agentRequestParser::resultType agentRequestParser::consume(request& req, char input) {
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

bool agentRequestParser::isChar(int c) {
	return c >= 0 && c <= 127;
}

bool agentRequestParser::isCtl(int c) {
	return (c >= 0 && c <= 31) || (c == 127);
}

bool agentRequestParser::isTspecial(int c) {
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

bool agentRequestParser::isDigit(int c) {
	return c >= '0' && c <= '9';
}