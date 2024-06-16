#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

#include "error.hpp"
#include "arguments.hpp"

void refresh(pgm::error &error) {
	const std::filesystem::directory_iterator power_supply_directory_iterator("/sys/class/power_supply");

	for (const std::filesystem::directory_entry &directory_entry : power_supply_directory_iterator) {
		std::cout << directory_entry.path() << std::endl;
	}
}

int main(int argc, char **argv) {
	pgm::error error;

	pgm::arguments arguments(argc, argv, error);
	if (error) {
		return error.print();
	}
	if (arguments.printed_usage) {
		return 0;
	}

	if (arguments.refresh_interval > 0) {
		const std::chrono::milliseconds refresh_interval(arguments.refresh_interval);
		
		std::chrono::time_point<std::chrono::steady_clock> next_refresh = std::chrono::steady_clock::now();
		for (;;) {
			std::this_thread::sleep_until(next_refresh);
			refresh(error);
			next_refresh += refresh_interval;
		}
	} else {
		refresh(error);
	}

}
