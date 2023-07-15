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