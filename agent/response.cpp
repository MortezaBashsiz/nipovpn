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
	boost::beast::error_code ec;
	boost::beast::http::response_parser<boost::beast::http::string_body> parser;
	parser.eager(true);
	parser.put(boost::asio::buffer(response), ec);
	boost::beast::http::response<boost::beast::http::string_body> res = parser.get();
	unsigned versionMajor = res.version() / 10;
	unsigned versionMinor = res.version() % 10;
	status =  std::string("HTTP/")+ std::to_string(versionMajor) + "." + std::to_string(versionMinor) 
						+ " " + boost::lexical_cast<std::string>(res.result_int()) 
						+ " " + boost::lexical_cast<std::string>(res.result()) + "\r\n";
	parsedResponse = res;
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
		std::cout << "H, " << headers[j].name << " : " << headers[j].value << std::endl;
		j+=1;
	}
	std::cout << "res recieved : " << std::endl << response << std::endl;
	std::cout << "res parsed : " << std::endl << boost::lexical_cast<std::string>(res) << std::endl;
	contentLength = res["Content-Size"];
}