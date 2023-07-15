#include "response.hpp"
#include "net.hpp"

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
}

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