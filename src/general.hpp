#include <iostream>
#include <yaml-cpp/yaml.h>

enum RunMode
{
	server,
	agent
};

static RunMode runMode_ = RunMode::server;

struct BoolStr
{
	bool ok;
	std::string message;
};

inline bool fileExists (const std::string& name);
const BoolStr validateConfig(int argc, char const *argv[]);