#include "response.hpp"
#include "net.hpp"

response response::stockResponse(response::statusType status) {
	response resp;
	resp.status = status;
	resp.responseBody.content = statusToString(status);
	resp.headers.resize(2);
	resp.headers[0].name = "Content-Length";
	resp.headers[0].value = std::to_string(resp.responseBody.content.size());
	resp.headers[1].name = "Content-Type";
	resp.headers[1].value = "text/html";
	return resp;
}