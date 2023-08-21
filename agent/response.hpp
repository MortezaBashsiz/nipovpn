#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <vector>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/process/async_pipe.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>

#include "header.hpp"

const char miscNameValueSeparator[] = { ':', ' ' };
const char miscCrlf[] = { '\r', '\n' };

struct response{
	std::string status;
	std::string content;
	std::string contentLength;
	std::vector<header> headers;
	std::vector<boost::asio::const_buffer> toBuffers();
	boost::beast::http::response<boost::beast::http::string_body> parsedResponse;
	void parse(std::string response);

	std::string toString()
	{
		return 	std::string("\n#######################################################################")
						+ "status : " + status
						+ "content : " + content + " \n"
						+ "contentLength : " + contentLength + " \n"
						+ "parsed headers : " + boost::lexical_cast<std::string>(parsedResponse.base()) + " \n"
						+"#######################################################################\n";
	}

};

#endif //RESPONSE_HPP