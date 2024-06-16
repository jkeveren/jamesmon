#include "arguments.hpp"

#include <string>
#include <iostream>
#include <format>

const std::string usage = "Usage: syspect [-i REFRESH_INTERVAL] [-f REFRESH_FREQUENCY]";

pgm::arguments::arguments(int argc, char **argv, error &error) {
	for (int i = 1; i < argc; i++) {
		std::string arg_string(argv[i]);

		// refresh interval
		if (arg_string == "-i") {
			std::string interval_string(argv[++i]);
			try {
				this->refresh_interval = std::stoi(interval_string);
			} catch (std::exception&) {
				error.append(std::format("Error parsing refresh interval \"{}\". Is it a number?", interval_string));
			}
			continue;
		}

		// refresh frequency
		if (arg_string == "-f") {
			std::string frequency_string(argv[++i]);
			int frequency = 0;
			try {
				frequency = std::stoi(frequency_string);
			} catch (std::exception&) {
				error.append(std::format("Error parsing refresh frequency \"{}\". Is it a number?", frequency_string));
			}
			this->refresh_interval = 1000 / frequency;
			continue;
		}

		std::cout << i << arg_string << usage << std::endl;
		printed_usage = true;
		break;
	}
}