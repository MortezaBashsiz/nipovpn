#include <iostream>
#include <yaml-cpp/yaml.h>


/*
* This enum defined the modes which program are able to start
* Mode server and agent
*/
enum RunMode
{
	server,
	agent
};


/*
* Main static variable to keep the mode
* See validateConfig in general.cpp where it will be defined as server or agent
*/
static RunMode runMode_ = RunMode::server;

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
inline bool fileExists (const std::string& name);

/*
* This function is responsible for validating the config
* It checks if all the blocks (log, agent, server) with all items are defined or not
* Returns the BoolStr structure, See general.cpp
*/
const BoolStr validateConfig(int argc, char const *argv[]);