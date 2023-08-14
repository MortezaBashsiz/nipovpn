#include "response.hpp"

std::vector<boost::asio::const_buffer> response::toBuffers() {
	std::vector<boost::asio::const_buffer> buffers;
	buffers.push_back(boost::asio::buffer("HTTP/1.1 200 OK\r\n"));
	for (std::size_t i = 0; i < headers.size(); ++i){
		header& h = headers[i];
		buffers.push_back(boost::asio::buffer(h.name));
		buffers.push_back(boost::asio::buffer(miscNameValueSeparator));
		buffers.push_back(boost::asio::buffer(h.value));
		buffers.push_back(boost::asio::buffer(miscCrlf));
	};
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
	for (res; !ec && boost::beast::http::read(pipe, buf, res, ec); res.clear()) {
		parsedResponse = res;
		contentLength = res["Content-Size"];
		encryptedContent = res.body().data();
		int i = 1;
		for (auto& h : res.base()) {
			i+=1;
		}
		headers.resize(i);
		int j = 0;
		for (auto& h : res.base()) {
			headers[j].name = h.name_string();
			headers[j].value = h.value();
			j+=1;
		}
	}
}