#include "net.hpp"

#include <string>

const std::string statusStringOk = "HTTP/1.0 200 OK\r\n";
const std::string statusStringBadRequest = "HTTP/1.0 400 Bad Request\r\n";
const std::string statusStringUnauthorized = "HTTP/1.0 401 Unauthorized\r\n";
const std::string statusStringForbidden = "HTTP/1.0 403 Forbidden\r\n";
const std::string statusStringNotFound = "HTTP/1.0 404 Not Found\r\n";
const std::string statusStringInternalServerError = "HTTP/1.0 500 Internal Server Error\r\n";
const std::string statusStringServiceUnavailable = "HTTP/1.0 503 Service Unavailable\r\n";

const char miscNameValueSeparator[] = { ':', ' ' };
const char miscCrlf[] = { '\r', '\n' };

asio::const_buffer statusToBuffer(response::statusType status){
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

std::string statusToString(response::statusType status){
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

std::vector<asio::const_buffer> response::toBuffers(){
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


response response::stockResponse(response::statusType status)
{
  response resp;
  resp.status = status;
  resp.responseBody.content = statusToString(status);
  resp.headers.resize(2);
  resp.headers[0].name = "Content-Length";
  resp.headers[0].value = std::to_string(resp.responseBody.content.size());
  resp.headers[1].name = "Content-Type";
  resp.headers[1].value = "text/html";
  return resp;
}