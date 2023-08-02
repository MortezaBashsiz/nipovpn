#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <vector>
#include <boost/asio.hpp>

struct response{
	std::string content;
	std::vector<boost::asio::const_buffer> toBuffers();
};

#endif //RESPONSE_HPP