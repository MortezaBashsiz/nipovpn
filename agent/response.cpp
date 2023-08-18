#include "response.hpp"

std::vector<boost::asio::const_buffer> response::toBuffers() {
	std::vector<boost::asio::const_buffer> buffers;
	buffers.push_back(boost::asio::buffer(status));
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
	// std::cout << "res recieved : " << std::endl << response << std::endl;
	boost::asio::io_context ctx;
	boost::process::async_pipe pipe(ctx);
	write(pipe, boost::asio::buffer(response));
	::close(pipe.native_sink());
	boost::beast::flat_buffer buf;
	boost::system::error_code ec;
	boost::beast::http::response<boost::beast::http::string_body> res;
	for (res; !ec && boost::beast::http::read(pipe, buf, res, ec); res.clear()) {
		// std::cout << "res version : " << std::endl << res.version() << std::endl;
		unsigned versionMajor = res.version() / 10;
		unsigned versionMinor = res.version() % 10;
		status =  std::string("HTTP/")+ std::to_string(versionMajor) + "." + std::to_string(versionMinor) 
							+ " " + boost::lexical_cast<std::string>(res.result_int()) 
							+ " " + boost::lexical_cast<std::string>(res.result()) + "\r\n";
		// std::cout << "res status : " << std::endl << status << std::endl;
		parsedResponse = res;
		encryptedContent = res.body().data();
		content = res.body().data();
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
		// std::cout << "res parsed : " << std::endl << boost::lexical_cast<std::string>(parsedResponse) << std::endl;
		contentLength = res["Content-Size"];
	}
}