#ifndef NET_HPP
#define NET_HPP

#include <string>
#include <vector>
#include <asio.hpp>

using namespace std;

struct body{
	enum bodyType{
		real = 1,
		fake = 2
	} type;	
	
	string content;
};

struct header{
	string name;
	string value;
};

struct request{
  string method;
  string uri;
  int httpVersionMajor;
  int httpVersionMinor;
  vector<header> headers;
  body requestBody;
};

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

	vector<header> headers;
	body responseBody;
	vector<asio::const_buffer> toBuffers();
	static response stockResponse(statusType status);
};

#endif // NET_HPP
