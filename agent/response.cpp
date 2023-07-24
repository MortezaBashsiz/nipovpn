#include "response.hpp"

std::vector<boost::asio::const_buffer> response::toBuffers() {
	std::vector<boost::asio::const_buffer> buffers;
	buffers.push_back(boost::asio::buffer(responseBody.content));
	return buffers;
};