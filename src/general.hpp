#pragma once
#ifndef GENERAL_HPP
#define GENERAL_HPP

#include <iostream>
#include <fstream>
#include <ctime>
#include <string>
#include <iomanip>

#include <yaml-cpp/yaml.h>

#include <boost/asio.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/algorithm/string.hpp>

/*
* General class to make the class uncopyable
*/
class Uncopyable {
public:
	Uncopyable(){}

private:
	Uncopyable(const Uncopyable&);
	Uncopyable& operator=(const Uncopyable&);
};

/*
* FUCK function prints it on screen
*/
inline void FUCK(const auto& message)
{
	std::cout << "FUCK FUCK FUCK FUCK FUCK FUCK FUCK FUCK : " << message << std::endl;
}

/*
* Struct BoolStr is used when you want to return a bool with a message
* Ussed in function validateConfig in general.cpp
*/
struct BoolStr
{
	bool ok;
	std::string message;
};

/*
* Simple function to check if the file exists or not
* Called from function validateConfig in general.cpp
*/
inline bool fileExists (const std::string& name) {
		std::ifstream file(name.c_str());
		return file.good();
}

/*
* This function is responsible for validating the config
* It checks if all the blocks (log, agent, server) with all items are defined or not
* Returns the BoolStr structure, See general.cpp
*/
inline BoolStr validateConfig(int argc, char const *argv[])
{
	/*
	* Initializing default return result
	*/
	BoolStr boolStr_{false, std::string("FAILED")};

	/*
	* Check if argc passed correctly it must be 3
	*/
	if (argc != 3)
	{
		boolStr_.message = 	std::string("We need 2 arguments to process, mode [server|agent] and config file path\n like : nipovpn server config.yaml\n");
		return boolStr_;
	}

	/*
	* Check if argv[1] is server or agent, If not then it is wrong
	*/
	if (argv[1] != std::string("server") && argv[1] != std::string("agent"))
	{
		boolStr_.message = std::string("Firts argument must be one) of [server|agent]\n");
		return boolStr_;
	}

	/*
	* Check if the config file exists or not
	*/
	if (! fileExists(argv[2]))
	{
		boolStr_.message = std::string("Specified config file ") + std::string(argv[2]) + " does not exists\n";
		return boolStr_;
	}

	/*
	* Main YAML node which includes the main parsed config.yaml
	*/
	YAML::Node configYaml_;
	std::string tmpStr("");

	/*
	* Check if the config is valid or not
	* First it checks if the yaml syntax is valid or not
	* Then it checks if all blocks (log, server, agent) and all directives are defined or not
	*/
	try
	{
		configYaml_ = YAML::LoadFile(argv[2]);
	} catch (std::exception& e)
	{
		boolStr_.message = std::string("Erro on parsing config file. : ") + e.what() + "\n";
		return boolStr_;
	}

	try
	{
		tmpStr = configYaml_["log"]["logFile"].as<std::string>();
		tmpStr = configYaml_["log"]["logLevel"].as<std::string>();
	} catch (std::exception& e)
	{
		boolStr_.message = std::string("Erro on parsing config file. something is wrong in block 'log' : ") + e.what() + "\n";
		return boolStr_;
	}

	try
	{
		tmpStr = configYaml_["server"]["listenIp"].as<std::string>();
		tmpStr = configYaml_["server"]["listenPort"].as<unsigned short>();
		tmpStr = configYaml_["server"]["webDir"].as<std::string>();
		tmpStr = configYaml_["server"]["sslKey"].as<std::string>();
		tmpStr = configYaml_["server"]["sslCert"].as<std::string>();
	} catch (std::exception& e)
	{
		boolStr_.message = std::string("Erro on parsing config file. something is wrong in block 'server' : ") + e.what() + "\n";
		return boolStr_;
	}

	try
	{
		tmpStr = configYaml_["agent"]["listenIp"].as<std::string>();
		tmpStr = configYaml_["agent"]["listenPort"].as<unsigned short>();
		tmpStr = configYaml_["agent"]["serverIp"].as<std::string>();
		tmpStr = configYaml_["agent"]["serverPort"].as<unsigned short>();
		tmpStr = configYaml_["agent"]["encryption"].as<std::string>();
		tmpStr = configYaml_["agent"]["token"].as<std::string>();
		tmpStr = configYaml_["agent"]["endPoint"].as<std::string>();
		tmpStr = configYaml_["agent"]["httpVersion"].as<unsigned short>();
		tmpStr = configYaml_["agent"]["userAgent"].as<std::string>();
	} catch (std::exception& e)
	{
		boolStr_.message = std::string("Erro on parsing config file. something is wrong in block 'agent' : ") + e.what() + "\n";
		return boolStr_;
	}

	boolStr_.ok =  true;
	boolStr_.message = "OK";
	return boolStr_;
}

inline std::string streambufToString(const boost::asio::streambuf& buff)
{
	std::string result(boost::asio::buffers_begin(buff.data()), boost::asio::buffers_begin(buff.data()) + buff.size());
	return result;
}

inline void copyStringToStreambuf(const std::string& inputStr, boost::asio::streambuf& buff)
{
	buff.consume(buff.size());
	std::iostream os(&buff);
	os << inputStr;
}

inline void moveStreamBuff(boost::asio::streambuf& source, boost::asio::streambuf& target)
{
	std::size_t bytes_copied = boost::asio::buffer_copy(
		target.prepare(source.size()), 
		source.data());
	target.commit(bytes_copied);
	source.consume(source.size());
}

inline void copyStreamBuff(boost::asio::streambuf& source, boost::asio::streambuf& target)
{
	std::size_t bytes_copied = boost::asio::buffer_copy(
		target.prepare(source.size()), 
		source.data());
	target.commit(bytes_copied);
}

inline std::string hexArrToStr(const unsigned char* data, const std::size_t& size)
{
	std::stringstream tempStr;
	tempStr << std::hex << std::setfill('0');
	for (long unsigned int i = 0; i < size; ++i)
	{
		tempStr << std::setw(2) << static_cast<unsigned>(data[i]);
	}
	return tempStr.str();
}

inline unsigned short hexToInt(const std::string& hexString)
{
	std::stringstream hexStr;
	unsigned short result;
	hexStr << std::hex << hexString;
	hexStr >> result;
	return result;
}

inline std::string hexToASCII(const std::string& hex)
{
	std::string ascii = "";
	for (size_t i = 0; i < hex.length(); i += 2){
		std::string part = hex.substr(i, 2);
		char ch = stoul(part, nullptr, 16);
		ascii += ch;
	}
	return ascii;
}

inline unsigned char charToHex(const char& c)
{
	if ('0' <= c && c <= '9') return c - '0';
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	return c - '0';
}

inline std::vector<unsigned char> strTohexArr(const std::string & hexStr)
{
	if (hexStr.size() % 2 != 0)
		return std::vector<unsigned char>{};
	std::vector<unsigned char> result(hexStr.size() / 2);

	for (std::size_t i = 0; i != hexStr.size() / 2; ++i)
		result[i] = 16 * charToHex(hexStr[2 * i]) + charToHex(hexStr[2 * i + 1]);

	return result;
}

inline std::vector<std::string> splitString(std::string str, std::string token){
	std::vector<std::string>result;
	while(str.size()){
			const long unsigned index = str.find(token);
			if(index!=std::string::npos){
					result.push_back(str.substr(0,index));
					str = str.substr(index+token.size());
					if(str.size()==0)result.push_back(str);
			}else{
					result.push_back(str);
					str = "";
			}
	}
	return result;
}

inline std::string decode64(const std::string& inputStr) 
{
	using binaryFromBase64 = boost::archive::iterators::binary_from_base64<std::string::const_iterator>;
	using transformWidth = boost::archive::iterators::transform_width<binaryFromBase64, 8, 6>;
	return boost::algorithm::trim_right_copy_if(
		std::string(
			transformWidth(std::begin(inputStr)),
			transformWidth(std::end(inputStr))
		), 
		[](char c) 
		{
			return c == '\0';
		}
	);
}

inline std::string encode64(const std::string& inputStr) 
{
	using transformWidth = boost::archive::iterators::transform_width<std::string::const_iterator, 6, 8>;
	using constIterators = boost::archive::iterators::base64_from_binary<transformWidth>;
	auto tmp = std::string(
		constIterators(std::begin(inputStr)),
		constIterators(std::end(inputStr))
	);
	return tmp.append((3 - inputStr.size() % 3) % 3, '=');
}

#endif /* GENERAL_HPP */