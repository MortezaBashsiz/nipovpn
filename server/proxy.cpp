#include "proxy.hpp"

Proxy::Proxy(Config config) : nipoLog(config)
{
	nipoConfig = config;
}

Proxy::~Proxy(){}

std::string Proxy::send(request request_)
{
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
			boost::beast::tcp_stream stream(ioc);
			stream.connect(results);
			if(ec && ec != boost::beast::errc::not_connected)
					throw boost::beast::system_error{ec};
			result = 	std::string("HTTP/1.0 200 Connection Established\r\n") +
								"Host: google.com\r\n"+
								"Accept: */*\r\n";
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