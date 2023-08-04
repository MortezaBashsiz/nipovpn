#include "proxy.hpp"

Proxy::Proxy(Config config) : nipoLog(config)
{
	nipoConfig = config;
}

Proxy::~Proxy(){}

std::string Proxy::send(std::string encryptedBody)
{
	boost::beast::http::response<boost::beast::http::string_body> res;
	try
	{
		boost::asio::io_context ioc;
		boost::asio::ip::tcp::resolver resolver(ioc);
		boost::beast::tcp_stream stream(ioc);
		auto const results = resolver.resolve(nipoConfig.config.serverIp, std::to_string(nipoConfig.config.serverPort));
		stream.connect(results);
		boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::post, nipoConfig.config.endPoint, nipoConfig.config.httpVersion, "\r\n"};
		req.set(boost::beast::http::field::host, nipoConfig.config.serverIp);
		req.set(boost::beast::http::field::user_agent, nipoConfig.config.userAgent);
		req.body() = encryptedBody;
		req.set(boost::beast::http::field::accept, "*/*");
		req.set(boost::beast::http::field::content_type, "application/javascript");
		req.set(boost::beast::http::field::content_length, std::to_string(req.body().size()));
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
	nipoLog.write("Request sent to nipoServer", nipoLog.levelDebug);
	return boost::lexical_cast<std::string>(res);
}