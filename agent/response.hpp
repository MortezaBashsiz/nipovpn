#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <vector>
#include <boost/asio.hpp>

struct ResponseBody{
	enum bodyType{
		real = 1,
		fake = 2
	} type;	
	
	std::string content;
};

struct response{
	ResponseBody responseBody;
	std::vector<boost::asio::const_buffer> toBuffers();
};

#endif //RESPONSE_HPP