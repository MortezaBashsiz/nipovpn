#ifndef NET_HPP
#define NET_HPP

#include <string>
#include <vector>
#include <set>
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

struct mimeMapping {
  const char* extension;
  const char* mimeType;
};

std::string mimeExtensionToType(const std::string& extension);

class requestHandler {
	public:
	  requestHandler(const requestHandler&) = delete;
	  requestHandler& operator=(const requestHandler&) = delete;
	  explicit requestHandler(const std::string& docRoot);
	  void handleRequest(const request& req, response& resp);
	
	private:
	  std::string docRoot_;
	  static bool urlDecode(const std::string& in, std::string& out);
};

class requestParser{
	public:
	  requestParser();
	
	  void reset();
	
	  enum resultType { good, bad, indeterminate };
	
	  template <typename InputIterator>
	  std::tuple<resultType, InputIterator> parse(request& req,
	      InputIterator begin, InputIterator end)
	  {
	    while (begin != end)
	    {
	      resultType result = consume(req, *begin++);
	      if (result == good || result == bad)
	        return std::make_tuple(result, begin);
	    }
	    return std::make_tuple(indeterminate, begin);
	  }
	
	private:
	  resultType consume(request& req, char input);
	
	  static bool isChar(int c);
	
	  static bool isCtl(int c);
	
	  static bool isTspecial(int c);
	
	  static bool isDigit(int c);
	
	  enum state
	  {
	    methodStart,
	    method,
	    uri,
	    httpVersionH,
	    httpVersionT1,
	    httpVersionT2,
	    httpVersionP,
	    httpVersionSlash,
	    httpVersionMajorStart,
	    httpVersionMajor,
	    httpVersionMinorStart,
	    httpVersionMinor,
	    expectingNewline1,
	    headerLineStart,
	    headerLws,
	    headerName,
	    spaceBeforeHeaderValue,
	    headerValue,
	    expectingNewline2,
	    expectingNewline3
	  } state_;
};

class connectionManager;

class connection : public std::enable_shared_from_this<connection> {
public:
  connection(const connection&) = delete;
  connection& operator=(const connection&) = delete;

  explicit connection(asio::ip::tcp::socket socket,
      connectionManager& manager, requestHandler& handler);

  void start();

  void stop();

private:
  void doRead();

  void doWrite();

  asio::ip::tcp::socket socket_;

  connectionManager& connectionManager_;

  requestHandler& requestHandler_;

  std::array<char, 8192> buffer_;

  request request_;

  requestParser requestParser_;

  response response_;
};

typedef std::shared_ptr<connection> connectionPtr;

class connectionManager
{
public:
  connectionManager(const connectionManager&) = delete;
  connectionManager& operator=(const connectionManager&) = delete;

  /// Construct a connection manager.
  connectionManager();

  /// Add the specified connection to the manager and start it.
  void start(connectionPtr c);

  /// Stop the specified connection.
  void stop(connectionPtr c);

  /// Stop all connections.
  void stopAll();

private:
  /// The managed connections.
  std::set<connectionPtr> connections_;
};

class server {
public:
  server(const server&) = delete;
  server& operator=(const server&) = delete;

  explicit server(const std::string& address, const std::string& port, const std::string& docRoot);

  void run();

private:
  void doAccept();

  void doAwaitStop();

  asio::io_context io_context_;

  asio::signal_set signals_;

  asio::ip::tcp::acceptor acceptor_;

  connectionManager connectionManager_;

  requestHandler requestHandler_;
};

#endif // NET_HPP
