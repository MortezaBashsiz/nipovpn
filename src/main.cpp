#include <iostream>

#include "config.hpp"

int main(int argc, char const *argv[])
{
	
	BoolStr configValidation{false, std::string("FAILED")};
	configValidation = validateConfig(argc, argv);
	if (! configValidation.ok)
	{
		std::cerr << configValidation.message;
		return 1;
	}

	Config nipoConfig(runMode_, std::string(argv[2]));

	return 0;
}