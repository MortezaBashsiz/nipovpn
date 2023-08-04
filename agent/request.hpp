#ifndef SERVERREQUEST_HPP
#define SERVERREQUEST_HPP

#include "config.hpp"
#include "log.hpp"
#include "response.hpp"


struct request{
	std::string clientIP;
	std::string clientPort;
	std::string content;

	std::string toString()
	{
		return 	"clientIP : " + clientIP + ", \n"
						+ "clientPort : " + clientPort + ", \n"
						+ "content : , \n " + content + "\n";
	}
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