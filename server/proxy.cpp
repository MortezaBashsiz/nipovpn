#include "proxy.hpp"

Proxy::Proxy(Config config) : nipoLog(config)
{
	nipoConfig = config;
}

Proxy::~Proxy(){}

std::string Proxy::send(proxyRequest request_)
{
	boost::beast::http::response<boost::beast::http::string_body> res;
	try
	{
		boost::asio::io_context ioc;
		boost::asio::ip::tcp::resolver resolver(ioc);
		boost::beast::tcp_stream stream(ioc);
		auto const results = resolver.resolve(request_.host, request_.port);
		stream.connect(results);
		boost::beast::http::verb verb;
		if (request_.method == "GET")
			verb = boost::beast::http::verb::get;
		else if (request_.method == "PUT")
			verb = boost::beast::http::verb::put;
		else if (request_.method == "POST")
			verb = boost::beast::http::verb::post;
		else if (request_.method == "CONNECT")
			verb = boost::beast::http::verb::connect;
		else if (request_.method == "HEAD")
			verb = boost::beast::http::verb::head;
		else if (request_.method == "OPTIONS")
			verb = boost::beast::http::verb::options;
		boost::beast::http::request<boost::beast::http::string_body> req{verb, request_.uri, request_.httpVersion, "\r\n"};
		req.set(boost::beast::http::field::host, request_.host);
		req.set(boost::beast::http::field::user_agent, request_.userAgent);
		req.body() = request_.content;
		for (std::size_t i = 0; i < request_.headers.size(); ++i)
		{
			req.set(request_.headers[i].name, request_.headers[i].value);
		}
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
	return boost::lexical_cast<std::string>(res);
}