#include <string>
#include <fstream>
#include <sstream>

#include "net.hpp"

asio::const_buffer statusToBuffer(response::statusType status) {
	switch (status){
	case response::ok:
		return asio::buffer(statusStringOk);
	case response::badRequest:
		return asio::buffer(statusStringBadRequest);
	case response::unauthorized:
		return asio::buffer(statusStringUnauthorized);
	case response::forbidden:
		return asio::buffer(statusStringForbidden);
	case response::notFound:
		return asio::buffer(statusStringNotFound);
	case response::internalServerError:
		return asio::buffer(statusStringInternalServerError);
	case response::serviceUnavailable:
		return asio::buffer(statusStringServiceUnavailable);
	default:
		return asio::buffer(statusStringInternalServerError);
	};
};

std::string statusToString(response::statusType status) {
	switch (status){
	case response::ok:
		return statusStringOk;
	case response::badRequest:
		return statusStringBadRequest;
	case response::unauthorized:
		return statusStringUnauthorized;
	case response::forbidden:
		return statusStringForbidden;
	case response::notFound:
		return statusStringNotFound;
	case response::internalServerError:
		return statusStringInternalServerError;
	case response::serviceUnavailable:
		return statusStringServiceUnavailable;
	default:
		return statusStringInternalServerError;
	};
};

std::vector<asio::const_buffer> response::toBuffers() {
	std::vector<asio::const_buffer> buffers;
	buffers.push_back(statusToBuffer(status));
	for (std::size_t i = 0; i < headers.size(); ++i){
		header& h = headers[i];
		buffers.push_back(asio::buffer(h.name));
		buffers.push_back(asio::buffer(miscNameValueSeparator));
		buffers.push_back(asio::buffer(h.value));
		buffers.push_back(asio::buffer(miscCrlf));
	};
	buffers.push_back(asio::buffer(miscCrlf));
	buffers.push_back(asio::buffer(responseBody.content));
	return buffers;
};

std::string mimeExtensionToType(const std::string& extension) {
	mimeMapping mimeMappings[] =
	{
	  { "gif", "image/gif" },
	  { "htm", "text/html" },
	  { "html", "text/html" },
	  { "jpg", "image/jpeg" },
	  { "png", "image/png" }
	};
	for (mimeMapping m: mimeMappings) {
		if (m.extension == extension) {
			return m.mimeType;
		};
	};
	return "text/plain";
};

requestHandler::requestHandler(Config nipoConfig, const std::string& docRoot) : docRoot_(docRoot), nipoLog(nipoConfig){
}

void requestHandler::handleRequest(const request& req, response& resp) {
	std::string requestPath;
	if (!urlDecode(req.uri, requestPath)) {
		resp = response::stockResponse(response::badRequest);
		return;
	};

	if (requestPath.empty() || requestPath[0] != '/' || requestPath.find("..") != std::string::npos) {
		resp = response::stockResponse(response::badRequest);
    string logMsg = "nipovpn request, " + req.method + ", " + req.uri + ", " + to_string(resp.responseBody.content.size()) + ", " + statusToString(resp.status);
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
    string logMsg = "nipovpn request, " + req.method + ", " + req.uri + ", " + to_string(resp.responseBody.content.size()) + ", " + statusToString(resp.status);
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
  string logMsg = "nipovpn request, " + req.method + ", " + req.uri + ", " + to_string(resp.responseBody.content.size()) + ", " + statusToString(resp.status);
  nipoLog.write(logMsg , nipoLog.levelInfo);
};

bool requestHandler::urlDecode(const std::string& in, std::string& out) {
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

requestParser::requestParser() : state_(methodStart) {
}

void requestParser::reset() {
  state_ = methodStart;
}

requestParser::resultType requestParser::consume(request& req, char input) {
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

bool requestParser::isChar(int c) {
  return c >= 0 && c <= 127;
}

bool requestParser::isCtl(int c) {
  return (c >= 0 && c <= 31) || (c == 127);
}

bool requestParser::isTspecial(int c) {
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

bool requestParser::isDigit(int c) {
  return c >= '0' && c <= '9';
}

connection::connection(asio::ip::tcp::socket socket,
    connectionManager& manager, requestHandler& handler)
  : socket_(std::move(socket)),
    connectionManager_(manager),
    requestHandler_(handler) {
}

void connection::start() {
  doRead();
}

void connection::stop() {
  socket_.close();
}

void connection::doRead() {
  auto self(shared_from_this());
  socket_.async_read_some(asio::buffer(buffer_),
      [this, self](std::error_code ec, std::size_t bytesTransferred) {
        if (!ec) {
          requestParser::resultType result;
          std::tie(result, std::ignore) = requestParser_.parse(
              request_, buffer_.data(), buffer_.data() + bytesTransferred);
          if (result == requestParser::good) {
            requestHandler_.handleRequest(request_, response_);
            doWrite();
          }
          else if (result == requestParser::bad) {
            response_ = response::stockResponse(response::badRequest);
            doWrite();
          }
          else {
            doRead();
          }
        }
        else if (ec != asio::error::operation_aborted) {
          connectionManager_.stop(shared_from_this());
        }
      });
}

void connection::doWrite() {
  auto self(shared_from_this());
  asio::async_write(socket_, response_.toBuffers(),
      [this, self](std::error_code ec, std::size_t)
      {
        if (!ec) {
          asio::error_code ignored_ec;
          socket_.shutdown(asio::ip::tcp::socket::shutdown_both,
            ignored_ec);
        }
        if (ec != asio::error::operation_aborted) {
          connectionManager_.stop(shared_from_this());
        }
      });
}

connectionManager::connectionManager(Config nipoConfig) : nipoLog(nipoConfig){
}

void connectionManager::start(connectionPtr c) {
  nipoLog.write("nipovpn started connectionManager", nipoLog.levelDebug);
  connections_.insert(c);
  c->start();
}

void connectionManager::stop(connectionPtr c) {
  nipoLog.write("nipovpn stopped connectionManager", nipoLog.levelDebug);
  connections_.erase(c);
  c->stop();
}

void connectionManager::stopAll() {
  nipoLog.write("nipovpn stopped All connectionManager", nipoLog.levelDebug);
  for (auto c: connections_)
    c->stop();
  connections_.clear();
}

server::server(Config nipoConfig)
  : io_context_(1),
    signals_(io_context_),
    acceptor_(io_context_),
    connectionManager_(nipoConfig),
    requestHandler_(nipoConfig, nipoConfig.config.webDir),
    nipoLog(nipoConfig) {

  signals_.add(SIGINT);
  signals_.add(SIGTERM);
#if defined(SIGQUIT)
  signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
  nipoLog.write("nipovpn started in server mode", nipoLog.levelInfo);
  doAwaitStop();
  asio::ip::tcp::resolver resolver(io_context_);
  asio::ip::tcp::endpoint endpoint = *resolver.resolve(nipoConfig.config.ip, std::to_string(nipoConfig.config.port)).begin();
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();
  doAccept();
};

void server::run() {
  io_context_.run();
};

void server::doAccept() {
  acceptor_.async_accept(
      [this](std::error_code ec, asio::ip::tcp::socket socket)
      {
        if (!acceptor_.is_open()) {
          return;
        };
        if (!ec) {
          connectionManager_.start(std::make_shared<connection>(
              std::move(socket), connectionManager_, requestHandler_));
        };
        doAccept();
      });
}

void server::doAwaitStop() {
  signals_.async_wait(
      [this](std::error_code /*ec*/, int /*signo*/)
      {
        acceptor_.close();
        connectionManager_.stopAll();
      });
}