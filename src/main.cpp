#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <ctime>
#include <fstream>
#include <format>

#include "error.hpp"
#include "arguments.hpp"

namespace pgm {
	// Get ifstreams for each of "/sys/devices/system/cpu/cpu*/cpuidle/state*/time".
	// Returns a 2D vector of cpus * states.
	std::vector<std::vector<std::ifstream>> get_cpu_idle_streams(pgm::error &error) {
		unsigned cpu_count = std::thread::hardware_concurrency();
		std::vector<std::vector<std::ifstream>> cpus;
		for (unsigned cpu_index = 0; cpu_index < cpu_count; cpu_index++) {
			std::vector<std::ifstream> state_streams;
			std::filesystem::directory_iterator idle_iterator(std::format("/sys/devices/system/cpu/cpu{}/cpuidle/", cpu_index));
			for (const std::filesystem::directory_entry& entry: idle_iterator) {
				std::filesystem::path path(entry.path() / "time");
				std::ifstream stream(path);
				if (!stream.is_open()) {
					error.append(std::format("Error opening file \"{}\"", path.string()));
					break;
				}
				state_streams.push_back(std::move(stream));
			}
			if (error) {
				break;
			}
			cpus.push_back(std::move(state_streams));
		}
		if (error) {
			error.append("Error getting streams for CPU idle time.");
		}
		return cpus;
	}

	void refresh(pgm::error &error) {
		// Reset the screen.
		std::cout
			<< "\x1B[2J" // clear
			"\x1B[H" // move to 0,0
		;

		// Timestamps
		std::cout
			<< "TAI:  " << std::chrono::tai_clock::now().time_since_epoch() << "\n"
			<< "UNIX: " << std::chrono::utc_clock::now().time_since_epoch() << "\n"
			"\n"
		;

		// Local Time
		std::time_t ctime = std::time(nullptr);
		std::tm *tm = std::localtime(&ctime);
		char local_time[50];
		std::strftime(local_time, sizeof(local_time) * sizeof(local_time[0]), "%F %b %a %T", tm);
		std::cout << local_time << "\n\n";

		// Uptime
		static std::ifstream uptime_ifstream("/proc/uptime");
		uptime_ifstream.seekg(0);
		float uptime;
		uptime_ifstream >> uptime;
		std::cout << "Uptime: " << std::fixed << std::setprecision(0) << uptime << "s (" << std::setprecision(5) << uptime / 86400 << "d)\n\n";

		// CPU
		std::cout << "CPUs: ";
		static pgm::error static_error;
		static std::vector<std::vector<std::ifstream>> cpus_idle_streams = get_cpu_idle_streams(static_error);
		if (static_error) {
			error = static_error;
			return;
		}
		for (std::vector<std::ifstream> &cpu_idle_streams: cpus_idle_streams) {
			long long cpu_idle_nanoseconds = 0;
			for (std::ifstream &stream: cpu_idle_streams) {
				stream.seekg(0);
				long long state_nanoseconds;
				stream >> state_nanoseconds;
				cpu_idle_nanoseconds += state_nanoseconds;
			}
			std::cout << cpu_idle_nanoseconds << " ";
		}

		std::cout << std::endl; // also flushes
	}
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
		refresh(error);
		if (error) {
			return error.print();
		}
		next_refresh += refresh_interval;
		std::this_thread::sleep_until(next_refresh);
	}

}
