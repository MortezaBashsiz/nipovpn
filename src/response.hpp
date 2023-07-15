#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <vector>
#include <asio.hpp>

#include "header.hpp"

struct response{
	enum statusType{
		ok = 200,
		badRequest = 400,
		unauthorized = 401,
		forbidden = 403,
		notFound = 404,
		internalServerError = 500,
		serviceUnavailable = 503
	} status;

	std::vector<header> headers;
	body responseBody;
	std::vector<asio::const_buffer> toBuffers();
	static response stockResponse(statusType status);
};

#endif //RESPONSE_HPP