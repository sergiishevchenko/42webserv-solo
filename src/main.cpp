#include <iostream>
#include <string>

static void printUsage(const char *progName) {
	std::cerr << "Usage: " << progName << " <configuration file>" << std::endl;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printUsage(argv[0]);
		return 1;
	}

	const std::string configPath = argv[1];
	std::cout << "webserv starting with config: " << configPath << std::endl;

	return 0;
}
