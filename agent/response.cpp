#include "response.hpp"

std::vector<boost::asio::const_buffer> response::toBuffers() {
	std::vector<boost::asio::const_buffer> buffers;
	buffers.push_back(boost::asio::buffer(miscCrlf));
	buffers.push_back(boost::asio::buffer(content));
	return buffers;

};

void response::parse(std::string response)
{
	boost::asio::io_context ctx;
	boost::process::async_pipe pipe(ctx);
	write(pipe, boost::asio::buffer(response));
	::close(pipe.native_sink());
	boost::beast::flat_buffer buf;
	boost::system::error_code ec;
	boost::beast::http::response<boost::beast::http::string_body> res;
	for (res; !ec && read(pipe, buf, res, ec); res.clear()) {
		parsedResponse = res;
		contentLength = res["Content-Length"];
		encryptedContent = res.body().data();
	}
}