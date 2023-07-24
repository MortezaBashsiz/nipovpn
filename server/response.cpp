#include "response.hpp"

boost::asio::const_buffer statusToBuffer(response::statusType status) {
	switch (status){
	case response::ok:
		return boost::asio::buffer(statusStringOk);
	case response::badRequest:
		return boost::asio::buffer(statusStringBadRequest);
	case response::unauthorized:
		return boost::asio::buffer(statusStringUnauthorized);
	case response::forbidden:
		return boost::asio::buffer(statusStringForbidden);
	case response::notFound:
		return boost::asio::buffer(statusStringNotFound);
	case response::internalServerError:
		return boost::asio::buffer(statusStringInternalServerError);
	case response::serviceUnavailable:
		return boost::asio::buffer(statusStringServiceUnavailable);
	default:
		return boost::asio::buffer(statusStringInternalServerError);
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

response response::stockResponse(response::statusType status) {
	response resp;
	resp.status = status;
	resp.responseBody.content = statusToString(status);
	resp.headers.resize(2);
	resp.headers[0].name = "Content-Length";
	resp.headers[0].value = std::to_string(resp.responseBody.content.size());
	resp.headers[1].name = "Content-Type";
	resp.headers[1].value = "text/html";
	return resp;
};

std::vector<boost::asio::const_buffer> response::toBuffers() {
	std::vector<boost::asio::const_buffer> buffers;
	buffers.push_back(statusToBuffer(status));
	for (std::size_t i = 0; i < headers.size(); ++i){
		header& h = headers[i];
		buffers.push_back(boost::asio::buffer(h.name));
		buffers.push_back(boost::asio::buffer(miscNameValueSeparator));
		buffers.push_back(boost::asio::buffer(h.value));
		buffers.push_back(boost::asio::buffer(miscCrlf));
	};
	buffers.push_back(boost::asio::buffer(miscCrlf));
	buffers.push_back(boost::asio::buffer(responseBody.content));
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