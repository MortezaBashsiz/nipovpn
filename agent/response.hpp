#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <vector>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/process/async_pipe.hpp>
#include <iostream>

struct response{
	std::string content;
	std::string encryptedContent;
	std::string contentLength;
	std::vector<boost::asio::const_buffer> toBuffers();
	boost::beast::http::response<boost::beast::http::string_body> parsedResponse;
	void parse(std::string response);
};



#endif //RESPONSE_HPP