#include "proxy.hpp"

// #include <boost/beast/core.hpp>
// #include <boost/beast/http.hpp>
// #include <boost/beast/version.hpp>
// #include <boost/asio/connect.hpp>
// #include <boost/asio/ip/tcp.hpp>

Proxy::Proxy(Config config) : nipoLog(config)
{
	nipoConfig = config;
}

Proxy::~Proxy(){}

boost::beast::http::response<boost::beast::http::string_body> Proxy::send(proxyRequest request_)
{
	boost::beast::http::response<boost::beast::http::string_body> res;
	try
	{
		boost::asio::io_context ioc;
		boost::asio::ip::tcp::resolver resolver(ioc);
		boost::beast::tcp_stream stream(ioc);
		auto const results = resolver.resolve(request_.host, request_.port);
		stream.connect(results);
		boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::get, request_.uri, request_.httpVersion, "\r\n"};
		req.set(boost::beast::http::field::host, request_.host);
		req.set(boost::beast::http::field::user_agent, request_.userAgent);
		req.body() = request_.content;
		req.set("Accept", "*/*");
		req.set("Content-Type", "application/javascript");
		req.set("Content-Length", std::to_string(req.body().length()));
		boost::beast::http::write(stream, req);
		boost::beast::flat_buffer buffer;
		boost::beast::http::read(stream, buffer, res);
		boost::beast::error_code ec;
		stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		if(ec && ec != boost::beast::errc::not_connected)
				throw boost::beast::system_error{ec};
	}
	catch(std::exception const& e)
	{
			std::cerr << "Error: " << e.what() << std::endl;
	}
	return res;
}