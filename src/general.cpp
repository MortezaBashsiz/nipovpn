#include <fstream>

#include "general.hpp"

inline bool fileExists (const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

const BoolStr validateConfig(int argc, char const *argv[])
{
	BoolStr boolStr_{false, std::string("FAILED")};
	if (argc != 3)
	{
		boolStr_.message = 	std::string("We need 2 arguments to process, mode [server|agent] and config file path\n like : nipovpn server config.yaml\n");
		return boolStr_;
	}
	if (argv[1] != std::string("server") && argv[1] != std::string("agent"))
	{
		boolStr_.message = std::string("Firts argument must be one) of [server|agent]\n");
		return boolStr_;
	}

	if (argv[1] == std::string("server"))
		runMode_ = RunMode::server;
	if (argv[1] == std::string("agent"))
		runMode_ = RunMode::agent;

	if (! fileExists(argv[2]))
	{
		boolStr_.message = std::string("Specified config file ") + std::string(argv[2]) + " does not exists\n";
		return boolStr_;	
	}

	YAML::Node configYaml_, logYaml_, agentYaml_, serverYaml_;

	try
	{
		configYaml_ = YAML::LoadFile(argv[2]);
	} catch (std::exception& e)
	{
		boolStr_.message = std::string("Erro on parsing config file. ") + e.what() + "\n";
		return boolStr_;
	}

	try
	{
		logYaml_ = configYaml_["log"];
	} catch (std::exception& e)
	{
		boolStr_.message = std::string("Erro on parsing config file. something is wrong in block 'log' ") + e.what() + "\n";
		return boolStr_;
	}
	
	try
	{
		agentYaml_ = configYaml_["agent"];
	} catch (std::exception& e)
	{
		boolStr_.message = std::string("Erro on parsing config file. something is wrong in block 'agent' ") + e.what() + "\n";
		return boolStr_;
	}

	try
	{
		serverYaml_ = configYaml_["server"];
	} catch (std::exception& e)
	{
		boolStr_.message = std::string("Erro on parsing config file. something is wrong in block 'server' ") + e.what() + "\n";
		return boolStr_;
	}

	boolStr_.ok =  true;
	boolStr_.message = "OK";
	return boolStr_;
}