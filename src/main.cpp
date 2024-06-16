#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <ctime>

#include "error.hpp"
#include "arguments.hpp"

void refresh([[maybe_unused]] pgm::error &error) {
	// Reset the screen.
	std::cout << /*clear*/"\x1B[2J" /*move to 0,0*/"\x1B[H";

	// Timestamps
	std::cout
		<< "TAI:  " << std::chrono::tai_clock::now().time_since_epoch() << "\n"
		<< "UNIX: " << std::chrono::utc_clock::now().time_since_epoch() << "\n"
	;

	// Local Time
	std::time_t ctime = std::time(nullptr);
	std::tm *tm = std::localtime(&ctime);
	char local_time[50];
	std::size_t size = strftime(local_time, sizeof(local_time) * sizeof(local_time[0]), "%F %b %a %T\n", tm);
	std::cout << local_time;

	std::flush(std::cout);

	// const std::filesystem::directory_iterator power_supply_directory_iterator("/sys/class/power_supply");
	// std::cout << std::distance(std::filesystem::begin(power_supply_directory_iterator), std::filesystem::end(power_supply_directory_iterator)) << std::endl;

	// for (const std::filesystem::directory_entry &directory_entry : power_supply_directory_iterator) {
	// 	std::cout << directory_entry.path() << std::endl;
	// }
}

int main(int argc, char **argv) {
	pgm::error error;

	// Parse Arguments
	pgm::arguments arguments(argc, argv, error);
	if (error) {
		return error.print();
	}
	// Exit if the result of arguments was to simply print usage.
	if (arguments.printed_usage) {
		return 0;
	}


	// Main Refresh Loop
	const std::chrono::milliseconds refresh_interval(arguments.refresh_interval);
	std::chrono::time_point<std::chrono::steady_clock> next_refresh = std::chrono::steady_clock::now();
	for (;;) {
		std::this_thread::sleep_until(next_refresh);
		refresh(error);
		if (error) {
			// Ignore for now
			error = pgm::error();
		}
		next_refresh += refresh_interval;
	}

}
