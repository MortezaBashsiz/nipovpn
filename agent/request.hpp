#ifndef SERVERREQUEST_HPP
#define SERVERREQUEST_HPP

#include "config.hpp"
#include "log.hpp"
#include "response.hpp"

struct RequestBody{
	enum bodyType{
		real = 1,
		fake = 2
	} type;	
	
	std::string content;
};

struct request{
	std::string clientIP;
	std::string clientPort;
	RequestBody requestBody;
};

class RequestHandler {
	public:
		RequestHandler(const RequestHandler&) = delete;
		RequestHandler& operator=(const RequestHandler&) = delete;
		explicit RequestHandler(Config config);
		void handleRequest(request& req, response& resp);
		Log nipoLog;
		Config nipoConfig;
};

#endif // SERVERREQUEST_HPP