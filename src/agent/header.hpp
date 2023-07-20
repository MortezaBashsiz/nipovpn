#ifndef HEADER_HPP
#define HEADER_HPP

#include <string>

struct body{
	enum bodyType{
		real = 1,
		fake = 2
	} type;	
	
	std::string content;
};

struct header{
	std::string name;
	std::string value;
};

#endif // HEADER_HPP