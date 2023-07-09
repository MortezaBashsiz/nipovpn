#include "general.h"

bool fileExists(string filePath){
	ifstream file;
	file.open(filePath);
	if (file) {
		file.close();
		return true;
	} else {
		return false;
	}
};