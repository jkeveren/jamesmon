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
	struct cpu_idle {
		long long previous_microseconds = 0;
		std::vector<std::ifstream> streams;
	};

	// Get ifstreams for each of "/sys/devices/system/cpu/cpu*/cpuidle/state*/time".
	// Returns a 2D vector of cpus * states.
	std::vector<cpu_idle>
	get_cpu_idles(pgm::error &error) {
		std::vector<cpu_idle> cpu_idles;
		unsigned cpu_count = std::thread::hardware_concurrency();
		for (unsigned cpu_index = 0; cpu_index < cpu_count; cpu_index++) {
			cpu_idle cpu_idle;
			std::filesystem::directory_iterator idle_iterator(std::format("/sys/devices/system/cpu/cpu{}/cpuidle/", cpu_index));
			for (const std::filesystem::directory_entry& entry: idle_iterator) {
				// std::string name;
				// std::ifstream(entry.path() / "name") >> name;
				// std::cout << name << std::endl;
				// if (name == "POLL" || name == "C1_ACPI" || name == "C2_ACPI" || name == "C3_ACPI") {
				// 	continue;
				// }

				std::filesystem::path path(entry.path() / "time");
				std::ifstream stream(path);
				if (!stream.is_open()) {
					error.append(std::format("Error opening file \"{}\"", path.string()));
					break;
				}
				cpu_idle.streams.push_back(std::move(stream));
			}
			if (error) {
				break;
			}
			cpu_idles.push_back(std::move(cpu_idle));
		}

		if (error) {
			error.append("Error getting streams for CPU idle time.");
		}
		// std::exit(6);
		return cpu_idles;
	}

	void
	refresh(pgm::error &error) {
		// Measure actual refresh interval.
		std::chrono::time_point time = std::chrono::steady_clock::now();
		static std::chrono::time_point previous_refresh_time = time;
		std::chrono::duration refresh_interval = time - previous_refresh_time;
		previous_refresh_time = time;
		long long interval_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(refresh_interval).count();

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
		std::cout << "Uptime: " << std::fixed << std::setprecision(0) << uptime << "s (" << std::setprecision(5) << uptime / 86400 /*seconds per day*/ << "d)\n\n";

		// CPU
		std::cout << "CPUs: ";
		static pgm::error static_error;
		static std::vector<pgm::cpu_idle> cpu_idles = get_cpu_idles(static_error);
		if (static_error) {
			error = static_error;
			return;
		}
		// print usage for each cpu
		for (cpu_idle &cpu_idle: cpu_idles) {
			// Sum idle time of all states
			long long idle_microseconds = 0;
			for (std::ifstream &stream: cpu_idle.streams) {
				stream.seekg(0);
				long long state_idle_microseconds;
				stream >> state_idle_microseconds;
				idle_microseconds += state_idle_microseconds;
			}

			long long delta_idle_microseconds = idle_microseconds - cpu_idle.previous_microseconds;
			cpu_idle.previous_microseconds = idle_microseconds;

			long long busy_microseconds = interval_microseconds - delta_idle_microseconds;
			std::cout << idle_microseconds << "\n" << interval_microseconds << "\n" << delta_idle_microseconds << "\n" << busy_microseconds << "\n\n";
		}

		std::cout << std::endl; // also flushes the stream.
	}
}

int
main(int argc, char **argv) {
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
