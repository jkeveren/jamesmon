#include "arguments.hpp"

#include <string>
#include <iostream>
#include <format>

const std::string usage = "Usage: jamesmon [-i REFRESH_INTERVAL_S | -f REFRESH_FREQUENCY_HZ] [-h] [--help] [-?]";

pgm::arguments::arguments(int argc, char **argv, error &error) {
	for (int i = 1; i < argc; i++) {
		std::string arg_string(argv[i]);

		// refresh interval
		if (arg_string == "-i") {
			// Assert that there is an argument specified before referencing argv[++i].
			if (i >= argc - 1) {
				error.append("-i specified but no interval provided.");
				break;
			}
			// parse argument
			std::string interval_string(argv[++i]);
			float interval; // seconds
			try {
				interval = std::stof(interval_string);
			} catch (std::exception&) {
				error.append(std::format("Error parsing refresh interval \"{}\". Is it a number?", interval_string));
			}
			refresh_interval = std::chrono::duration<float>(interval);
			continue;
		}

		// refresh frequency
		if (arg_string == "-f") {
			// Assert that there is an argument specified before referencing argv[++i].
			if (i >= argc - 1) {
				error.append("-f specified but no frequency provided.");
				break;
			}
			// parse argument
			std::string frequency_string(argv[++i]);
			float frequency;
			try {
				frequency = std::stof(frequency_string);
			} catch (std::exception&) {
				error.append(std::format("Error parsing refresh frequency \"{}\". Is it a number?", frequency_string));
			}
			refresh_interval = std::chrono::duration<float>(1) / frequency;
			continue;
		}

		// help
		// We print usage if an unrecognised argument is used anyway so there's no point in checking for "-h" etc.
		// Would be nice to set error codes appropriately but it's unnecessary for this more human oriented tool.
		std::cout << usage << std::endl;
		printed_usage = true;
		break;
	}
	
	if (error) {
		error.append("Error parsing arguments.");
	}
}