#include "request.hpp"
#include "response.hpp"

RequestHandler::RequestHandler(Config config) : nipoLog(config){
	nipoConfig = config;
}

void RequestHandler::handleRequest(request& req, response& resp) {
	char buf[512];
	resp.responseBody.content = "DONE\n";
	std::string logMsg = 	"request, "
												+ req.clientIP + ":" 
												+ req.clientPort + ", "
												+ to_string(resp.responseBody.content.size());
	nipoLog.write(logMsg , nipoLog.levelInfo);
};