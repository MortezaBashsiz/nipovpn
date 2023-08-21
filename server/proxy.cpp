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
			boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls);
			ssl_context.set_default_verify_paths();
			boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket(ioc, ssl_context);
			boost::asio::connect(socket.next_layer(), results);
			socket.set_verify_mode(boost::asio::ssl::verify_peer);
			socket.set_verify_callback(boost::asio::ssl::host_name_verification(request_.host));
			if(! SSL_set_tlsext_host_name(socket.native_handle(), request_.host.c_str()))
			{
			  throw boost::system::system_error(
			      ::ERR_get_error(), boost::asio::error::get_ssl_category());
			}
			socket.handshake(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::client);
			result = "HTTP/1.0 200 Connection Established\r\n";
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