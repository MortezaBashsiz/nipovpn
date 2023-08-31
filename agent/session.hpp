#ifndef SESSION_HPP
#define SESSION_HPP

#include <set>

#include "config.hpp"
#include "log.hpp"
#include "response.hpp"
#include "request.hpp"
#include "encrypt.hpp"
#include "proxy.hpp"

class SessionManager;

class Session : public std::enable_shared_from_this<Session> {
	public:
		Session(const Session&) = delete;
		Session& operator=(const Session&) = delete;
		explicit Session(boost::asio::ip::tcp::socket socket, SessionManager& manager, RequestHandler& handler, Config config);
		void start();
		void stop();
		Config nipoConfig;
		Log nipoLog;
		Encrypt nipoEncrypt;
		Proxy nipoProxy;
	
	private:
		void doRead();
		void doWrite();
		boost::asio::ip::tcp::socket socket_;
		SessionManager& SessionManager_;
		RequestHandler& RequestHandler_;
		std::array<char, 8192> buffer_;
		request request_;
		response response_;
};

typedef std::shared_ptr<Session> SessionPtr;

class SessionManager
{
	public:
		SessionManager(const SessionManager&) = delete;
		SessionManager& operator=(const SessionManager&) = delete;
		SessionManager(Config config);
		void start(SessionPtr c);
		void stop(SessionPtr c);
		void stopAll();
		Log nipoLog;
		Config nipoConfig;
	
	private:
		std::set<SessionPtr> sessions_;
};

#endif //SESSION_HPP