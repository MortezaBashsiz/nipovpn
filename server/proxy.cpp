#include "proxy.hpp"

Proxy::Proxy(Config config) : nipoLog(config)
{
	nipoConfig = config;
}

Proxy::~Proxy(){}

std::string Proxy::send(request request_){
	boost::beast::http::response<boost::beast::http::string_body> res;
	std::string result;
	try
	{
		boost::asio::io_context ioc;
		boost::beast::flat_buffer buffer;
		boost::beast::error_code ec;
		boost::asio::ip::tcp::resolver resolver(ioc);
		auto const results = resolver.resolve(request_.host, request_.port);
		if ( request_.method == boost::beast::http::verb::connect)
		{
			try {
				boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls);
				ssl_context.set_default_verify_paths();
				boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket(ioc, ssl_context);
				boost::asio::connect(socket.next_layer(), results);
				result = "HTTP/1.1 200 Connection established\r\n";
			} 
			catch(std::exception const& e) {
				std::cerr << "Error: " << e.what() << std::endl;
				result = "HTTP/1.1 503 Service Unavailable\r\n";
			}
		} 
		else 
		{
			boost::beast::tcp_stream stream(ioc);
			stream.connect(results);
			boost::beast::http::write(stream, request_.parsedRequest);
			boost::beast::http::read(stream, buffer, res);
			stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			if(ec && ec != boost::beast::errc::not_connected)
					throw boost::beast::system_error{ec};
			result = boost::lexical_cast<std::string>(res);
		}
	}
	catch(std::exception const& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
	}
	return result;
}

std::string Proxy::sendClientHello(std::string data, std::string server, std::string port){
	nipoLog.write("Request sent to originserver", nipoLog.levelDebug);
	boost::beast::flat_buffer buffer;
	boost::asio::io_service svc;
	Client client(svc, server, port);

	std::vector<unsigned char> result(data.size() / 2);
	for (std::size_t i = 0; i != data.size() / 2; ++i)
		result[i] = 16 * charToHex(data[2 * i]) + charToHex(data[2 * i + 1]);
	return client.send(result);
}

Client::Client(boost::asio::io_service& svc, std::string const& host, std::string const& port)
		: io_service(svc), socket(io_service) {
	boost::asio::ip::tcp::resolver resolver(io_service);
	boost::asio::ip::tcp::resolver::iterator endpoint = resolver.resolve(boost::asio::ip::tcp::resolver::query(host, port));
	boost::asio::connect(this->socket, endpoint);
};

std::string Client::send(std::vector<unsigned char> message) {
	boost::system::error_code ec;
	boost::asio::write(socket, boost::asio::buffer(message), ec);
	if (ec) {
		std::cerr << ec.what();
	}
	boost::asio::streambuf responseBuffer;
	int bytesTransferred = boost::asio::read(socket, responseBuffer, boost::asio::transfer_all(), ec);
	if (ec && ec != boost::asio::error::eof) {
		std::cerr << ec.what();
	}
	unsigned char tempData[bytesTransferred];
	std::memcpy(tempData, boost::asio::buffer_cast<const void*>(responseBuffer.data()), bytesTransferred);
	// std::cout << "Payload: " << std::endl;
	// for (int i = 0; i < bytesTransferred; ++i) {
	// 	printf("%02x ", tempData[i]);
	// 	if ((i + 1) % 8 == 0) {
	// 		std::cout << "   ";
	// 	}
	// 	if ((i + 1) % 16 == 0) {
	// 		std::cout << std::endl;
	// 	}
	// }
	// std::cout << std::endl;
	// std::cout << "DEBUG : " << bytesTransferred << std::endl;
	std::stringstream tempStr;
	tempStr << std::hex << std::setfill('0');
	for (int i = 0; i < bytesTransferred; ++i)
	{
		tempStr << std::setw(2) << static_cast<unsigned>(tempData[i]);
	}
	return tempStr.str();
}